// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/PosixSignals.h>
#include <xzero/logging.h>
#include <signal.h>
#include <assert.h>

namespace xzero {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("PosixSignals", msg)
#define DEBUG(msg...) logDebug("PosixSignals", msg)
#else
#define TRACE(msg...) do {} while (0)
#define DEBUG(msg...) do {} while (0)
#endif

PosixSignals* PosixSignals::singleton_ = nullptr;

PosixSignals::PosixSignals(Executor* executor)
    : executor_(executor), watchers_(128), mutex_() {
  assert(singleton_ == nullptr && "Must be a singleton.");
  singleton_ = this;
}

PosixSignals::~PosixSignals() {
  singleton_ = nullptr;
}

PosixSignals::HandleRef PosixSignals::notify(int signo, SignalHandler task) {
  std::lock_guard<std::mutex> _l(mutex_);

  RefPtr<SignalWatcher> hr(new SignalWatcher(task));

  if (watchers_[signo].empty()) {
    TRACE("installing signal handler");
    signal(signo, &PosixSignals::onSignal);
  }

  watchers_[signo].emplace_back(hr);

  return hr.as<Executor::Handle>();
}

void PosixSignals::onSignal(int signo) {
  singleton_->onSignal2(signo);
}

void PosixSignals::onSignal2(int signo) {
  signal(signo, SIG_DFL);

  std::vector<RefPtr<SignalWatcher>> pending;
  {
    std::lock_guard<std::mutex> _l(mutex_);
    std::list<RefPtr<SignalWatcher>>& watchers = watchers_[signo];
    pending.reserve(watchers.size());
    pending.insert(pending.end(), watchers.begin(), watchers.end());
    watchers.clear();
  }

  TRACE("caught signal $0 ($1) for $2 handlers", signo,
        toString(signo), pending.size());

  // notify interests (XXX must not be invoked on local stack)
  for (const auto& hr : pending)
    executor_->execute(std::bind(&SignalWatcher::fire, hr));
}

PosixSignals* PosixSignals::get() {
  return singleton_;
}

}  // namespace xzero
