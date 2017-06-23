// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/KQueueSignals.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

KQueueSignals::KQueueSignals(Executor* executor)
    : executor_(executor),
      fd_(kqueue()),
      oldSignalMask_(),
      watchers_(128),
      interests_(0),
      mutex_() {
  sigprocmask(SIG_SETMASK, nullptr, &oldSignalMask_);
}

KQueueSignals::~KQueueSignals() {
  // restore old signal mask
  sigprocmask(SIG_SETMASK, &oldSignalMask_, nullptr);
}

KQueueSignals::HandleRef KQueueSignals::notify(int signo, SignalHandler task) {
  std::lock_guard<std::mutex> _l(mutex_);

  // add event to kqueue
  if (watchers_[signo].empty()) {
    struct kevent ke;
    EV_SET(&ke, signo, EVFILT_SIGNAL, EV_ADD | EV_ONESHOT,
           0 /* fflags */,
           0 /* data */,
           nullptr /* udata */);
    int rv = kevent(fd_, &ke, 1, nullptr, 0, nullptr);
    if (rv < 0) {
      RAISE_ERRNO(errno);
    }

    blockSignal(signo);
  }

  RefPtr<SignalWatcher> hr(new SignalWatcher(task));
  watchers_[signo].emplace_back(hr);

  if (interests_.load() == 0) {
    executor_->executeOnReadable(fd_, std::bind(&KQueueSignals::onSignal, this));
    executor_->unref();
  }

  interests_++;

  return hr.as<Executor::Handle>();
}

void KQueueSignals::onSignal() {
  executor_->ref();

  // collect pending signals
  timespec timeout;
  timeout.tv_sec = 0;
  timeout.tv_nsec = 0;
  struct kevent events[16];

  int rv = 0;
  for (;;) {
    rv = kevent(fd_, nullptr, 0, events, 128, &timeout);
    if (rv < 0) {
      if (errno == EINTR) {
        continue;
      }
      RAISE_ERRNO(errno);
    }
    break;
  }

  std::vector<RefPtr<SignalWatcher>> pending;
  pending.reserve(rv);
  {
    // move pending signals out of the watchers
    std::lock_guard<std::mutex> _l(mutex_);
    for (int i = 0; i < rv; ++i) {
      int signo = events[i].ident;
      std::list<RefPtr<SignalWatcher>>& watchers = watchers_[signo];

      logDebug("UnixSignals", "Caught signal $0.", toString(signo));

      for (RefPtr<SignalWatcher>& watcher: watchers) {
        watcher->info.signal = signo;
      }

      interests_ -= watchers_[signo].size();
      pending.insert(pending.end(), watchers.begin(), watchers.end());
      watchers.clear();
    }

    // reregister for further signals, if anyone interested
    if (interests_.load() > 0) {
      executor_->executeOnReadable(fd_, std::bind(&KQueueSignals::onSignal, this));
      executor_->unref();
    }
  }

  // notify interests
  for (RefPtr<SignalWatcher>& hr: pending)
    safeCall_.invoke(std::bind(&SignalWatcher::fire, hr));
}
