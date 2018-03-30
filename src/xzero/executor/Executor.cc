// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/Executor.h>
#include <xzero/StringUtil.h>
#include <xzero/thread/Wakeup.h>
#include <fmt/format.h>
#include <iostream>

namespace xzero {

Executor::Executor(ExceptionHandler eh)
    : safeCall_{std::move(eh)},
      refs_(0) {
}

void Executor::setExceptionHandler(ExceptionHandler eh) {
  safeCall_.setExceptionHandler(std::move(eh));
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

std::ostream& operator<<(std::ostream& os, Executor* executor) {
  os << fmt::format("Executor@{:p} <{}>", (void*) executor, executor->toString());
  return os;
}

} // namespace xzero
