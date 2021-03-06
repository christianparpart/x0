// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Application.h>
#include <xzero/UnixTime.h>
#include <xzero/WallClock.h>
#include <xzero/defines.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/executor/ThreadPool.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <xzero/thread/Wakeup.h>

#include <exception>
#include <system_error>
#include <thread>
#include <typeinfo>

#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace xzero {

ThreadPool::ThreadPool()
    : ThreadPool(Application::processorCount(), nullptr) {
}

ThreadPool::ThreadPool(size_t num_threads)
    : ThreadPool(num_threads, nullptr) {
}

ThreadPool::ThreadPool(ExceptionHandler eh)
    : ThreadPool(Application::processorCount(), std::move(eh)) {
}

ThreadPool::ThreadPool(size_t num_threads, ExceptionHandler eh)
    : Executor{std::move(eh)},
      active_{true},
      threads_{},
      mutex_{},
      condition_{},
      pendingTasks_{},
      activeTasks_{0},
      activeTimers_{0},
      activeReaders_{0},
      activeWriters_{0} {

  if (num_threads < 1)
    throw std::invalid_argument{"num_threads"};

  for (size_t i = 0; i < num_threads; i++) {
    threads_.emplace_back(std::bind(&ThreadPool::work, this, i));
  }
}

ThreadPool::~ThreadPool() {
  stop();

  for (std::thread& thread: threads_) {
    thread.join();
  }
}

size_t ThreadPool::pendingCount() const {
  std::unique_lock<std::mutex> lock(mutex_);
  return pendingTasks_.size();
}

size_t ThreadPool::activeCount() const {
  return activeTasks_;
}

void ThreadPool::wait() {
  std::unique_lock<std::mutex> lock(mutex_);

  if (pendingTasks_.empty() && activeTasks_ == 0) {
    return;
  }

  condition_.wait(lock, [&]() -> bool {
    return pendingTasks_.empty() && activeTasks_.load() == 0;
  });
}

void ThreadPool::stop() {
  active_ = false;
  condition_.notify_all();
}

void ThreadPool::work(int workerId) {
  while (active_) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(lock, [&]() { return !pendingTasks_.empty() || !active_; });

      if (!active_)
        break;

      task = std::move(pendingTasks_.front());
      pendingTasks_.pop_front();
    }

    activeTasks_++;
    safeCall(task);
    activeTasks_--;

    // notify the potential wait() call
    condition_.notify_all();
  }
}

void ThreadPool::execute(Task task) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingTasks_.emplace_back(std::move(task));
  }
  condition_.notify_all();
}

ThreadPool::HandleRef ThreadPool::executeOnReadable(const Socket& s, Task task, Duration tmo, Task tcb) {
  // TODO: honor timeout
  HandleRef hr = std::make_shared<Handle>(nullptr);
  activeReaders_++;
  execute([this, task, hr, s = std::cref(s)] {
    PosixScheduler::waitForReadable(s);
    safeCall([&] { hr->fire(task); });
    activeReaders_--;
  });
  return nullptr;
}

ThreadPool::HandleRef ThreadPool::executeOnWritable(const Socket& s, Task task, Duration tmo, Task tcb) {
  // TODO: honor timeout
  HandleRef hr = std::make_shared<Handle>(nullptr);
  activeWriters_++;
  execute([this, task, hr, s = std::cref(s)] {
    PosixScheduler::waitForWritable(s);
    safeCall([&] { hr->fire(task); });
    activeWriters_--;
  });
  return hr;
}

ThreadPool::HandleRef ThreadPool::executeAfter(Duration delay, Task task) {
  HandleRef hr = std::make_shared<Handle>(nullptr);
  activeTimers_++;
  execute([this, task, hr, delay] {
    std::this_thread::sleep_for(std::chrono::microseconds{ delay.microseconds() });
    safeCall([&] { hr->fire(task); });
    activeTimers_--;
  });
  return hr;
}

ThreadPool::HandleRef ThreadPool::executeAt(UnixTime dt, Task task) {
  HandleRef hr = std::make_shared<Handle>(nullptr);
  activeTimers_++;
  execute([this, task, hr, dt] {
    UnixTime now = WallClock::now();
    if (dt > now) {
      Duration delay = dt - now;
      std::this_thread::sleep_for(std::chrono::microseconds{ delay.microseconds() });
    }
    safeCall([&] { hr->fire(task); });
    activeTimers_--;
  });
  return hr;
}

void ThreadPool::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  activeTimers_++;
  wakeup->onWakeup(generation, [this, task] {
    execute(task);
    activeTimers_--;
  });
}

std::string ThreadPool::toString() const {
  char buf[32];

  int n = snprintf(buf, sizeof(buf), "ThreadPool(%zu)@%p", threads_.size(),
                   this);

  return std::string(buf, n);
}

} // namespace xzero
