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
      epollfd_(),
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
}

Executor::HandleRef LinuxScheduler::executeAfter(Duration delay, Task task) {
}

Executor::HandleRef LinuxScheduler::executeAt(UnixTime dt, Task task) {
}

Executor::HandleRef LinuxScheduler::executeOnReadable(int fd, Task task, Duration tmo, Task tcb) {
}

Executor::HandleRef LinuxScheduler::executeOnWritable(int fd, Task task, Duration tmo, Task tcb) {
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
