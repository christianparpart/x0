// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/ThreadPool.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/thread/Wakeup.h>
#include <xzero/Application.h>
#include <xzero/WallClock.h>
#include <xzero/UnixTime.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <system_error>
#include <thread>
#include <exception>
#include <typeinfo>

#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace xzero {

template<typename... Args> constexpr void TRACE(const char* msg, Args... args) {
#ifndef NDEBUG
  ::xzero::logTrace(std::string("ThreadPool: ") + msg, args...);
#endif
}

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
  TRACE("{} wait()", (void*) this);
  std::unique_lock<std::mutex> lock(mutex_);

  if (pendingTasks_.empty() && activeTasks_ == 0) {
    TRACE("{} wait: pending={}, active={} (immediate return)",
          (void*) this, pendingTasks_.size(), activeTasks_.load());
    return;
  }

  condition_.wait(lock, [&]() -> bool {
    TRACE("{} wait: pending={}, active={}",
          (void*) this, pendingTasks_.size(), activeTasks_.load());
    return pendingTasks_.empty() && activeTasks_.load() == 0;
  });
}

void ThreadPool::stop() {
  active_ = false;
  condition_.notify_all();
}

std::string ThreadPool::getThreadName(const void* tid) {
  // musl libc doesn't support that ;-(
#if defined(HAVE_DECL_PTHREAD_GETNAME_NP) && HAVE_DECL_PTHREAD_GETNAME_NP
  char name[16];
  name[0] = '\0';
  pthread_getname_np(*(const pthread_t*)tid, name, sizeof(name));
  return name;
#else
  return Application::appName();
#endif
}

void ThreadPool::work(int workerId) {
  TRACE("{} worker[{}] enter", (void*) this, workerId);

  while (active_) {
    Task task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(lock, [&]() { return !pendingTasks_.empty() || !active_; });

      if (!active_)
        break;

      TRACE("{} work[{}]: task received", (void*) this, workerId);
      task = std::move(pendingTasks_.front());
      pendingTasks_.pop_front();
    }

    activeTasks_++;
    safeCall(task);
    activeTasks_--;

    // notify the potential wait() call
    condition_.notify_all();
  }

  TRACE("{} worker[{}] leave", (void*) this, workerId);
}

void ThreadPool::execute(Task task) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    TRACE("{} execute: enqueue task & notify_all", (void*) this);
    pendingTasks_.emplace_back(std::move(task));
  }
  condition_.notify_all();
}

ThreadPool::HandleRef ThreadPool::executeOnReadable(int fd, Task task, Duration tmo, Task tcb) {
  // TODO: honor timeout
  HandleRef hr = std::make_shared<Handle>(nullptr);
  activeReaders_++;
  execute([this, task, hr, fd] {
    PosixScheduler::waitForReadable(fd);
    safeCall([&] { hr->fire(task); });
    activeReaders_--;
  });
  return nullptr;
}

ThreadPool::HandleRef ThreadPool::executeOnWritable(int fd, Task task, Duration tmo, Task tcb) {
  // TODO: honor timeout
  HandleRef hr = std::make_shared<Handle>(nullptr);
  activeWriters_++;
  execute([this, task, hr, fd] {
    PosixScheduler::waitForWritable(fd);
    safeCall([&] { hr->fire(task); });
    activeWriters_--;
  });
  return hr;
}

void ThreadPool::cancelFD(int fd) {
}

ThreadPool::HandleRef ThreadPool::executeAfter(Duration delay, Task task) {
  HandleRef hr = std::make_shared<Handle>(nullptr);
  activeTimers_++;
  execute([this, task, hr, delay] {
    usleep(delay.microseconds());
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
      usleep(delay.microseconds());
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

void ThreadPool::run(std::function<void()> task) {
  execute(std::move(task));
}

void ThreadPool::runOnReadable(std::function<void()> task, int fd) {
  executeOnReadable(fd, std::move(task));
}

void ThreadPool::runOnWritable(std::function<void()> task, int fd) {
  executeOnWritable(fd, std::move(task));
}

void ThreadPool::runOnWakeup(std::function<void()> task,
                             Wakeup* wakeup,
                             long generation) {
  executeOnWakeup(std::move(task), wakeup, generation);
}

} // namespace xzero
