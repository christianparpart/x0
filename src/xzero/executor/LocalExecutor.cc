// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/LocalExecutor.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <cstdio>

namespace xzero {

LocalExecutor::LocalExecutor(
    bool recursive,
    ExceptionHandler eh)
    : Executor{std::move(eh)},
      recursive_{recursive},
      running_{0},
      deferred_{} {
}

void LocalExecutor::execute(Task task) {
  if (isRunning() && !isRecursive()) {
    deferred_.emplace_back(std::move(task));
    return;
  }

  running_++;
  safeCall(task);
  executeDeferredTasks();
  running_--;
}

void LocalExecutor::executeDeferredTasks() {
  while (!deferred_.empty()) {
    safeCall(deferred_.front());
    deferred_.pop_front();
  }
}

Executor::HandleRef LocalExecutor::executeOnReadable(const Socket& s, Task task, Duration timeout, Task onTimeout) {
  try {
    PosixScheduler::waitForReadable(s, timeout);
  } catch (...) {
    onTimeout();
    return nullptr;
  }
  task();
  return nullptr;
}

Executor::HandleRef LocalExecutor::executeOnWritable(const Socket& s, Task task, Duration timeout, Task onTimeout) {
  try {
    PosixScheduler::waitForWritable(s, timeout);
  } catch (...) {
    onTimeout();
    return nullptr;
  }
  task();
  return nullptr;
}

Executor::HandleRef LocalExecutor::executeAfter(Duration delay, Task task) {
  logFatal("NotImplementedError"); // TODO
}

Executor::HandleRef LocalExecutor::executeAt(UnixTime ts, Task task) {
  logFatal("NotImplementedError"); // TODO
}

void LocalExecutor::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  logFatal("NotImplementedError"); // TODO
}

std::string LocalExecutor::toString() const {
  char buf[128];
  snprintf(buf, sizeof(buf), "LocalExecutor@%p", this);
  return buf;
}

} // namespace xzero
