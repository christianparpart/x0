// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/Executor.h>
#include <xzero/UnixSignals.h>
#include <xzero/StringUtil.h>
#include <xzero/thread/Wakeup.h>

namespace xzero {

Executor::Executor(std::unique_ptr<xzero::ExceptionHandler> eh)
    : safeCall_(std::move(eh)),
      unixSignals_(UnixSignals::create(this)),
      refs_(0) {
}

Executor::~Executor() {
  unixSignals_.reset();
}

void Executor::setExceptionHandler(std::unique_ptr<ExceptionHandler> eh) {
  safeCall_.setExceptionHandler(std::move(eh));
}

Executor::HandleRef Executor::executeOnSignal(int signo, SignalHandler task) {
  return unixSignals_->executeOnSignal(signo, task);
}

/**
 * Run the provided task when the wakeup handle is woken up
 */
void Executor::executeOnNextWakeup(Task  task, Wakeup* wakeup) {
  executeOnWakeup(task, wakeup, wakeup->generation());
}

/**
 * Run the provided task when the wakeup handle is woken up
 */
void Executor::executeOnFirstWakeup(Task task, Wakeup* wakeup) {
  executeOnWakeup(task, wakeup, 0);
}

void Executor::safeCall(std::function<void()> callee) noexcept {
  safeCall_.invoke(callee);
}

template<>
std::string StringUtil::toString(Executor* executor) {
  char buf[256];
  snprintf(buf, sizeof(buf), "Executor@%p <%s>",
           executor, executor->toString().c_str());
  return buf;
}

} // namespace xzero
