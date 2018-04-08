// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/ThreadedExecutor.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/executor/ThreadPool.h>
#include <xzero/thread/Wakeup.h>
#include <xzero/Application.h>
#include <xzero/RuntimeError.h>
#include <xzero/WallClock.h>
#include <xzero/sysconfig.h>
#include <memory>
#include <algorithm>
#include <limits>

#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace xzero {

ThreadedExecutor::ThreadedExecutor(ExceptionHandler eh)
    : Executor(eh),
      threads_() {
}

ThreadedExecutor::~ThreadedExecutor() {
  joinAll();
}

void ThreadedExecutor::joinAll() {
  for (;;) {
    std::thread t;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (threads_.empty())
        break;

      t = std::move(threads_.front());
      threads_.pop_front();
    }
    t.join();
  }
}

void ThreadedExecutor::setThreadName(const std::string& name) {
#if defined(HAVE_PTHREAD_H)
    pthread_t tid = pthread_self();
#if defined(HAVE_DECL_PTHREAD_SETNAME_NP) && HAVE_DECL_PTHREAD_SETNAME_NP
# if XZERO_OS_DARWIN
    // on Darwin you can only set thread rhead name for your own thread
    pthread_setname_np(name.c_str());
# else
    pthread_setname_np(tid, name.c_str());
# endif
#endif
#endif
}

void ThreadedExecutor::execute(const std::string& name, Task task) {
  auto runner = [this, name, task]() {
    setThreadName(name);
    safeCall(task);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto i = std::find_if(threads_.begin(), threads_.end(), [](std::thread& t) {
          return t.get_id() == std::this_thread::get_id();
      });
      if (i != threads_.end()) {
        i->detach();
        threads_.erase(i);
      }
    }
  };

  std::lock_guard<std::mutex> lock(mutex_);
  threads_.emplace_back(runner);
}

void ThreadedExecutor::execute(Task task) {
  execute(Application::appName(), task);
}

Executor::HandleRef ThreadedExecutor::executeOnReadable(int fd, Task task, Duration timeout, Task onTimeout) {
  PosixScheduler::waitForReadable(fd, timeout);
  // TODO if (timedout) { onTimeout(); return; }
  task();

  return Executor::HandleRef(); // TODO
}

Executor::HandleRef ThreadedExecutor::executeOnWritable(int fd, Task task, Duration timeout, Task onTimeout) {
  logFatal("NotImplementedError"); // TODO
}

void ThreadedExecutor::cancelFD(int fd) {
  logFatal("NotImplementedError"); // TODO
}

Executor::HandleRef ThreadedExecutor::executeAfter(Duration delay, Task task) {
  HandleRef hr = std::make_shared<Handle>(nullptr);
  execute([this, task, hr, delay] {
    usleep(delay.microseconds());
    safeCall([&] { hr->fire(task); });
  });
  return hr;
}

Executor::HandleRef ThreadedExecutor::executeAt(UnixTime dt, Task task) {
  HandleRef hr = std::make_shared<Handle>(nullptr);
  execute([this, task, hr, dt] {
    UnixTime now = WallClock::now();
    if (dt > now) {
      Duration delay = dt - now;
      usleep(delay.microseconds());
    }
    safeCall([&] { hr->fire(task); });
  });
  return hr;
}

void ThreadedExecutor::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  wakeup->onWakeup(generation, [this, task] {
    execute(task);
  });
}

std::string ThreadedExecutor::toString() const {
  char buf[32];
  snprintf(buf, sizeof(buf), "ThreadedExecutor@%p", this);
  return buf;
}

} // namespace xzero
