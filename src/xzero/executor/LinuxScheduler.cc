// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/LinuxScheduler.h>
#include <xzero/MonotonicTime.h>
#include <xzero/MonotonicClock.h>
#include <xzero/WallClock.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>

namespace xzero {

LinuxScheduler::LinuxScheduler(
    std::unique_ptr<xzero::ExceptionHandler> eh,
    std::function<void()> preInvoke,
    std::function<void()> postInvoke)
    : EventLoop(std::move(eh)), 
      onPreInvokePending_(preInvoke),
      onPostInvokePending_(postInvoke),
      epollfd_(epoll_create1(EPOLL_CLOEXEC)),
      eventfd_(),
      timerfd_(),
      signalfd_(),
      readerCount_(0),
      writerCount_(0)
{
}

LinuxScheduler::LinuxScheduler(
    std::unique_ptr<xzero::ExceptionHandler> eh)
    : LinuxScheduler(std::move(eh), nullptr, nullptr) {
}

LinuxScheduler::LinuxScheduler()
    : LinuxScheduler(nullptr, nullptr, nullptr) {
}

LinuxScheduler::~LinuxScheduler() {
}

MonotonicTime LinuxScheduler::now() const {
  return MonotonicClock::now();
}

void LinuxScheduler::execute(Task task) {
}

std::string LinuxScheduler::toString() const {
  return "LinuxScheduler";
}

Executor::HandleRef LinuxScheduler::executeAfter(Duration delay, Task task) {
  return nullptr;
}

Executor::HandleRef LinuxScheduler::executeAt(UnixTime dt, Task task) {
  return nullptr;
}

Executor::HandleRef LinuxScheduler::executeOnReadable(int fd, Task task,
                                                      Duration tmo, Task tcb) {
  std::lock_guard<std::mutex> lk(lock_);
  return createWatcher(Mode::READABLE, fd, task, tmo, tcb);
}

Executor::HandleRef LinuxScheduler::executeOnWritable(int fd, Task task,
                                                      Duration tmo, Task tcb) {
  std::lock_guard<std::mutex> lk(lock_);
  return createWatcher(Mode::WRITABLE, fd, task, tmo, tcb);
}

Executor::HandleRef LinuxScheduler::createWatcher(
    Mode mode, int fd, Task task, Duration tmo, Task tcb) {

  MonotonicTime timeout = now() + tmo;

  std::unordered_map<int, Watcher>::iterator wi = watchers_.find(fd);
  if (wi != watchers_.end() && wi->second.fd >= 0) {
    RAISE(AlreadyWatchingOnResource, "Already watching on resource");
  }

  Watcher* interest = &watchers_[fd];

  interest->setCancelHandler([this, interest] () {
    std::lock_guard<std::mutex> _l(lock_);
    unlinkWatcher(interest);
  });

  switch (mode) {
    case Mode::READABLE:
      readerCount_++;
      break;
    case Mode::WRITABLE:
      writerCount_++;
      break;
  }

  return linkWatcher(interest, findPrecedingWatcher(interest));
}

LinuxScheduler::Watcher* LinuxScheduler::findPrecedingWatcher(Watcher* interest) {
  if (firstWatcher_ == nullptr)
    // put it in front (linked list empty currently anyway)
    return nullptr;

  // find closest predecessor in timeout-ASC-ordered linked list
  Watcher* succ = lastWatcher_;
  while (succ->prev != nullptr) {
    if (succ->prev->timeout < interest->timeout) {
      // put it after that
      return succ->prev;
    } else {
      succ = succ->prev;
    }
  }

  // the timeout of @p interest is smaller than anything else
  // in the timeout-ASC-ordered list (or list was just 1 item)
  if (firstWatcher_->timeout < interest->timeout) {
    // put in back
    return lastWatcher_;
  } else {
    // put in front
    return nullptr;
  }
}

LinuxScheduler::Watcher* LinuxScheduler::linkWatcher(Watcher* w, Watcher* pred) {
  Watcher* succ = pred ? pred->next : firstWatcher_;

  w->prev = pred;
  w->next = succ;

  if (pred)
    pred->next = w;
  else
    firstWatcher_ = w;

  if (succ)
    succ->prev = w;
  else
    lastWatcher_ = w;

  ref();

  wakeupLoop();

  return w;
}

LinuxScheduler::Watcher* LinuxScheduler::unlinkWatcher(Watcher* w) {
  Watcher* pred = w->prev;
  Watcher* succ = w->next;

  if (pred)
    pred->next = succ;
  else
    firstWatcher_ = succ;

  if (succ)
    succ->prev = pred;
  else
    lastWatcher_ = pred;

  w->clear();
  unref();

  return succ;
}

void LinuxScheduler::cancelFD(int fd) {
}

void LinuxScheduler::executeOnWakeup(Task task, Wakeup* wakeup, long generation) {
}

void LinuxScheduler::runLoop() {
}

void LinuxScheduler::runLoopOnce() {
}

void LinuxScheduler::breakLoop() {
}

} // namespace xzero
