// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/executor/Executor.h>
#include <deque>

namespace xzero {

/**
 * Executor to directly invoke the tasks being passed by the caller.
 *
 * @note Not thread-safe.
 */
class LocalExecutor : public Executor {
 public:
  LocalExecutor(
    bool recursive = false,
    std::unique_ptr<xzero::ExceptionHandler> eh = nullptr);

  std::string toString() const override;
  void execute(Task task) override;
  HandleRef executeOnReadable(int fd, Task task, Duration timeout, Task onTimeout) override;
  HandleRef executeOnWritable(int fd, Task task, Duration timeout, Task onTimeout) override;
  void cancelFD(int fd) override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime ts, Task task) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;

  /** Tests whether this executor is currently running some task. */
  bool isRunning() const { return running_ > 0; }

  /** Tests whether this executor allows recursive execution of tasks. */
  bool isRecursive() const { return recursive_; }

  /** Sets wether recursive execution is allowed or flattened. */
  void setRecursive(bool value) { recursive_ = value; }

  /** Retrieves number of deferred tasks. */
  size_t backlog() const { return deferred_.size(); }

  void executeDeferredTasks();

 private:
  bool recursive_;
  int running_;
  std::deque<Task> deferred_;
};

} // namespace xzero
