// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <fmt/format.h>
#include <xzero/PosixSignals.h>
#include <xzero/StringUtil.h>
#include <xzero/executor/Executor.h>
#include <xzero/thread/Wakeup.h>
#include <xzero/PosixSignals.h>

#include <iostream>

namespace xzero {

Executor::Executor(ExceptionHandler eh)
    : safeCall_{std::move(eh)},
      signals_{},
      refs_(0) {
}

Executor::~Executor() {
}

void Executor::setExceptionHandler(ExceptionHandler eh) {
  safeCall_.setExceptionHandler(std::move(eh));
}

Executor::HandleRef Executor::executeOnSignal(int signo, SignalHandler handler) {
  if (!signals_)
    signals_ = std::make_unique<PosixSignals>(this);

  return signals_->notify(signo, [this, handler](const auto& si) { execute(std::bind(handler, si)); });
}

/**
 * Run the provided task when the wakeup handle is woken up
 */
void Executor::executeOnNextWakeup(Task  task, Wakeup* wakeup) {
  executeOnWakeup(std::move(task), wakeup, wakeup->generation());
}

/**
 * Run the provided task when the wakeup handle is woken up
 */
void Executor::executeOnFirstWakeup(Task task, Wakeup* wakeup) {
  executeOnWakeup(std::move(task), wakeup, 0);
}

void Executor::safeCall(std::function<void()> callee) noexcept {
  safeCall_.invoke(std::move(callee));
}

} // namespace xzero
