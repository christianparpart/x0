// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/LocalExecutor.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <stdio.h>

namespace xzero {

#ifndef NDEBUG
#define TRACE(msg...) logTrace("executor.LocalExecutor", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

LocalExecutor::LocalExecutor(
    bool recursive,
    std::unique_ptr<xzero::ExceptionHandler> eh)
    : Executor(std::move(eh)),
      recursive_(recursive),
      running_(0),
      deferred_() {
}

void LocalExecutor::execute(Task task) {
  if (isRunning() && !isRecursive()) {
    deferred_.emplace_back(std::move(task));
    TRACE("$0 execute: enqueue task ($1)", this, deferred_.size());
    return;
  }

  running_++;
  TRACE("$0 execute: run top-level task", this);
  safeCall(task);
  executeDeferredTasks();
  running_--;
}

void LocalExecutor::executeDeferredTasks() {
  while (!deferred_.empty()) {
    TRACE("$0 execute: run deferred task (-$1)", this, deferred_.size());
    safeCall(deferred_.front());
    deferred_.pop_front();
  }
}

Executor::HandleRef LocalExecutor::executeOnReadable(int fd, Task task, Duration timeout, Task onTimeout) {
  try {
    PosixScheduler::waitForReadable(fd, timeout);
  } catch (...) {
    onTimeout();
    return nullptr;
  }
  task();
  return nullptr;
}

Executor::HandleRef LocalExecutor::executeOnWritable(int fd, Task task, Duration timeout, Task onTimeout) {
  try {
    PosixScheduler::waitForWritable(fd, timeout);
  } catch (...) {
    onTimeout();
    return nullptr;
  }
  task();
  return nullptr;
}

void LocalExecutor::cancelFD(int fd) {
  RAISE(NotImplementedError); // TODO
}

Executor::HandleRef LocalExecutor::executeAfter(Duration delay, Task task) {
  RAISE(NotImplementedError); // TODO
}

Executor::HandleRef LocalExecutor::executeAt(UnixTime ts, Task task) {
  RAISE(NotImplementedError); // TODO
}

void LocalExecutor::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  RAISE(NotImplementedError); // TODO
}

std::string LocalExecutor::toString() const {
  char buf[128];
  snprintf(buf, sizeof(buf), "LocalExecutor@%p", this);
  return buf;
}

template<>
std::string StringUtil::toString(LocalExecutor* value) {
  return StringUtil::format("LocalExecutor[$0]", (void*)value);
}

} // namespace xzero
