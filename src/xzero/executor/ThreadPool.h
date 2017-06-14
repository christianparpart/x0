// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/executor/Executor.h>
#include <xzero/ExceptionHandler.h>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <atomic>
#include <deque>

namespace xzero {

/**
 * Standard thread-safe thread pool.
 */
class ThreadPool : public Executor {
 public:
  /**
   * Initializes this thread pool as many threads as CPU cores are available.
   */
  ThreadPool();

  /**
   * Initializes this thread pool.
   * @param num_threads number of threads to allocate.
   */
  explicit ThreadPool(size_t num_threads);

  /**
   * Initializes this thread pool as many threads as CPU cores are available.
   */
  explicit ThreadPool(std::unique_ptr<xzero::ExceptionHandler> eh);

  /**
   * Initializes this thread pool.
   *
   * @param num_threads number of threads to allocate.
   */
  ThreadPool(size_t num_threads,
             std::unique_ptr<xzero::ExceptionHandler> error_handler);

  ~ThreadPool();

  static size_t processorCount();

  static std::string getThreadName(const void* id);

  /**
   * Retrieves the number of pending tasks.
   */
  size_t pendingCount() const;

  /**
   * Retrieves the number of threads currently actively running a task.
   */
  size_t activeCount() const;

  /**
   * Notifies all worker threads to stop after their current job (if any).
   */
  void stop();

  /**
   * Waits until all jobs are processed.
   */
  void wait();

  using Executor::executeOnReadable;
  using Executor::executeOnWritable;

  // overrides
  void execute(Task task) override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime dt, Task task) override;
  HandleRef executeOnReadable(int fd, Task task, Duration tmo, Task tcb) override;
  HandleRef executeOnWritable(int fd, Task task, Duration tmo, Task tcb) override;
  void cancelFD(int fd) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;
  std::string toString() const override;

  // compatibility layer to old ThreadPool API
  XZERO_DEPRECATED void run(std::function<void()> task);
  XZERO_DEPRECATED void runOnReadable(std::function<void()> task, int fd);
  XZERO_DEPRECATED void runOnWritable(std::function<void()> task, int fd);
  XZERO_DEPRECATED void runOnWakeup(std::function<void()> task,
                                  Wakeup* wakeup,
                                  long generation);

 private:
  void work(int workerId);

 private:
  bool active_;
  std::deque<std::thread> threads_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::deque<Task> pendingTasks_;
  std::atomic<size_t> activeTasks_;
  std::atomic<size_t> activeTimers_;
  std::atomic<size_t> activeReaders_;
  std::atomic<size_t> activeWriters_;
};

} // namespace xzero
