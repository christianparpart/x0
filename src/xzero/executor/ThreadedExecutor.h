// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/executor/Executor.h>
#include <deque>
#include <pthread.h>
#include <mutex>

namespace xzero {

/**
 * Executor Service using threads.
 *
 * Every executed task will be getting its own dedicated
 * system thread.
 */
class ThreadedExecutor : public Executor {
 public:
  ThreadedExecutor() : ThreadedExecutor(nullptr) {}
  explicit ThreadedExecutor(std::unique_ptr<xzero::ExceptionHandler> eh);
  ~ThreadedExecutor();

  void execute(const std::string& name, Task task);

  void execute(Task task) override;
  HandleRef executeOnReadable(int fd, Task task, Duration timeout, Task onTimeout) override;
  HandleRef executeOnWritable(int fd, Task task, Duration timeout, Task onTimeout) override;
  void cancelFD(int fd) override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime ts, Task task) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;
  std::string toString() const override;

  void joinAll();

 private:
  static void* launchme(void* ptr);

  std::deque<pthread_t> threads_;
  std::mutex mutex_;
};

} // namespace xzero

