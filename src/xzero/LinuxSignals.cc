// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/LinuxSignals.h>
#include <xzero/logging.h>
#include <sys/signalfd.h>
#include <unistd.h>

namespace xzero {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("LinuxSignals", msg)
#define DEBUG(msg...) logDebug("LinuxSignals", msg)
#else
#define TRACE(msg...) do {} while (0)
#define DEBUG(msg...) do {} while (0)
#endif

LinuxSignals::LinuxSignals(Executor* executor)
    : executor_(executor),
      fd_(),
      signalMask_(),
      mutex_(),
      interests_(0),
      watchers_(128) {
  sigemptyset(&signalMask_);
}

LinuxSignals::~LinuxSignals() {
}

UnixSignals::HandleRef LinuxSignals::notify(int signo, SignalHandler task) {
  std::lock_guard<std::mutex> _l(mutex_);

  sigaddset(&signalMask_, signo);

  // on-demand create signalfd instance / update signalfd's mask
  if (fd_.isOpen()) {
    int rv = signalfd(fd_, &signalMask_, 0);
    if (rv < 0) {
      RAISE_ERRNO(errno);
    }
  } else {
    fd_ = signalfd(-1, &signalMask_, SFD_NONBLOCK | SFD_CLOEXEC);
    if (fd_ < 0) {
      RAISE_ERRNO(errno);
    }
    TRACE("notify: signalfd=$0", fd_.get());
  }

  // block this signal also to avoid default disposition
  sigprocmask(SIG_BLOCK, &signalMask_, nullptr);

  TRACE("notify: $0", toString(signo));
  RefPtr<SignalWatcher> hr(new SignalWatcher(task));
  watchers_[signo].emplace_back(hr);

  if (interests_.load() == 0) {
    executor_->executeOnReadable(fd_, std::bind(&LinuxSignals::onSignal, this));
    executor_->unref();
  }

  interests_++;

  return hr.as<Executor::Handle>();
}

void LinuxSignals::onSignal() {
  std::lock_guard<std::mutex> _l(mutex_);
  executor_->ref();

  sigset_t clearMask;
  sigemptyset(&clearMask);

  signalfd_siginfo events[16];
  ssize_t n = 0;
  for (;;) {
    n = read(fd_, events, sizeof(events));
    if (n < 0) {
      if (errno == EINTR)
        continue;

      RAISE_ERRNO(errno);
    }
    break;
  }

  // move pending signals out of the watchers
  std::vector<RefPtr<SignalWatcher>> pending;
  n /= sizeof(*events);
  pending.reserve(n);
  {
    for (int i = 0; i < n; ++i) {
      const signalfd_siginfo& event = events[i];
      int signo = event.ssi_signo;
      std::list<RefPtr<SignalWatcher>>& watchers = watchers_[signo];

      DEBUG("Caught signal $0 from PID $1 UID $2.",
            toString(signo),
            event.ssi_pid,
            event.ssi_uid);

      for (RefPtr<SignalWatcher>& watcher: watchers) {
        watcher->info.signal = signo;
        watcher->info.pid = event.ssi_pid;
        watcher->info.uid = event.ssi_uid;
      }

      sigdelset(&signalMask_, signo);
      interests_ -= watchers_[signo].size();
      pending.insert(pending.end(), watchers.begin(), watchers.end());
      watchers.clear();
    }

    // reregister for further signals, if anyone interested
    if (interests_.load() > 0) {
      executor_->executeOnReadable(fd_, std::bind(&LinuxSignals::onSignal, this));
      executor_->unref();
    }
  }

  // update signal mask
  sigprocmask(SIG_BLOCK, &signalMask_, nullptr);
  signalfd(fd_, &signalMask_, 0);

  // notify interests (XXX must not be invoked on local stack)
  for (RefPtr<SignalWatcher>& hr: pending)
    executor_->execute(std::bind(&SignalWatcher::fire, hr));
}

} // namespace xzero
