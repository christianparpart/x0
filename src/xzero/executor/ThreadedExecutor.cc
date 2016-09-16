// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/ThreadedExecutor.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/executor/ThreadPool.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>
#include <memory>
#include <algorithm>
#include <limits>
#include <pthread.h>

namespace xzero {

#if 0 //!defined(NDEBUG)
static std::mutex m;
#define TRACE(msg...)  do { \
    m.lock(); \
    printf("ThreadedExecutor: " msg); \
    printf("\n"); \
    m.unlock(); \
  } while (0);
#else
#define TRACE(msg...) do { } while (0)
#endif

ThreadedExecutor::ThreadedExecutor(std::unique_ptr<xzero::ExceptionHandler> eh)
    : Executor(std::move(eh)),
      threads_() {
}

ThreadedExecutor::~ThreadedExecutor() {
  joinAll();
}

void ThreadedExecutor::joinAll() {
  for (;;) {
    pthread_t tid = 0;
    {
      TRACE("joinAll: getting lock for getting TID");
      std::lock_guard<std::mutex> lock(mutex_);
      if (threads_.empty())
        break;

      tid = threads_.front();
      threads_.pop_front();
    }
    TRACE("joinAll: join($0) $1", tid, ThreadPool::getThreadName(&tid));
    pthread_join(tid, nullptr);
  }
  TRACE("joinAll: done");
}

void* ThreadedExecutor::launchme(void* ptr) {
  TRACE("launchme[$0]($1) enter", pthread_self(), ptr);
  std::unique_ptr<Executor::Task> task(reinterpret_cast<Executor::Task*>(ptr));
  (*task)();
  TRACE("launchme[$0]($1) leave", pthread_self(), ptr);
  return nullptr;
}

void ThreadedExecutor::execute(const std::string& name, Task task) {
  pthread_t tid;
  auto runner = [this, name, task]() {
#if defined(HAVE_DECL_PTHREAD_SETNAME_NP)
# if XZERO_OS_DARWIN
    pthread_setname_np(name.c_str());
# else
    pthread_setname_np(pthread_self(), name.c_str());
# endif
#endif
    safeCall(task);
  };
  pthread_create(&tid, NULL, &launchme, new Task{std::move(runner)});

  std::lock_guard<std::mutex> lock(mutex_);
  threads_.push_back(tid);
}

void ThreadedExecutor::execute(Task task) {
  pthread_t tid = 0;
  //pthread_create(&tid, NULL, &launchme, new Task{std::move(task)});
  pthread_create(&tid, NULL, &launchme, new Task([this, task]{
    pthread_t tid = pthread_self();
    safeCall(task);
    {
      TRACE("task $0 finished. getting lock for cleanup", ThreadPool::getThreadName(&tid));
      std::lock_guard<std::mutex> lock(mutex_);
      pthread_detach(tid);
      auto i = std::find(threads_.begin(), threads_.end(), tid);
      if (i != threads_.end()) {
        threads_.erase(i);
      }
    }
  }));
  std::lock_guard<std::mutex> lock(mutex_);
  threads_.push_back(tid);
}

Executor::HandleRef ThreadedExecutor::executeOnReadable(int fd, Task task, Duration timeout, Task onTimeout) {
  PosixScheduler::waitForReadable(fd, timeout);
  // TODO if (timedout) { onTimeout(); return; }
  task();

  return Executor::HandleRef(); // TODO
}

Executor::HandleRef ThreadedExecutor::executeOnWritable(int fd, Task task, Duration timeout, Task onTimeout) {
  RAISE(NotImplementedError); // TODO
}

void ThreadedExecutor::cancelFD(int fd) {
  RAISE(NotImplementedError); // TODO
}

Executor::HandleRef ThreadedExecutor::executeAfter(Duration delay, Task task) {
  RAISE(NotImplementedError); // TODO
}

Executor::HandleRef ThreadedExecutor::executeAt(UnixTime ts, Task task) {
  RAISE(NotImplementedError); // TODO
}

void ThreadedExecutor::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
  RAISE(NotImplementedError); // TODO
}

std::string ThreadedExecutor::toString() const {
  char buf[32];
  snprintf(buf, sizeof(buf), "ThreadedExecutor@%p", this);
  return buf;
}

} // namespace xzero
