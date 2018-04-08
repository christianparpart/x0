// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/PosixSignals.h>
#include <xzero/logging.h>
#include <csignal>
#include <cassert>

namespace xzero {

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

  std::shared_ptr<SignalWatcher> hr = std::make_shared<SignalWatcher>(task);

  if (watchers_[signo].empty()) {
#if defined(XZERO_OS_WIN32)
    signal(signo, &PosixSignals::onSignal);
#else
    struct sigaction sa{};
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = PosixSignals::onSignal;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigaction(signo, &sa, nullptr);
#endif
  }

  watchers_[signo].emplace_back(hr);

  return hr;
}

#if defined(XZERO_OS_WIN32)
void PosixSignals::onSignal(int signo) {
  singleton_->onSignal2(signo, 0, 0, nullptr);
}
#else
void PosixSignals::onSignal(int signo, siginfo_t* info, void* ptr) {
  singleton_->onSignal2(signo, info->si_pid, info->si_uid, ptr);
}
#endif

void PosixSignals::onSignal2(int signo, int pid, int uid, void* ptr) {
  std::vector<std::shared_ptr<SignalWatcher>> pending;
  {
    std::lock_guard<std::mutex> _l(mutex_);
    std::list<std::shared_ptr<SignalWatcher>>& watchers = watchers_[signo];
    pending.reserve(watchers.size());
    pending.insert(pending.end(), watchers.begin(), watchers.end());
    watchers.clear();
  }

  for (std::shared_ptr<SignalWatcher>& p: pending) {
    p->info.signal = signo;
    p->info.pid = pid;
    p->info.uid = uid;
  }

  // notify interests (XXX must not be invoked on local stack)
  for (const auto& hr : pending)
    executor_->execute(std::bind(&SignalWatcher::fire, hr));
}

PosixSignals* PosixSignals::get() {
  return singleton_;
}

}  // namespace xzero
