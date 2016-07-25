// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/RefPtr.h>
#include <xzero/MonotonicTime.h>
#include <xzero/executor/EventLoop.h>
#include <xzero/io/FileDescriptor.h>
#include <vector>
#include <set>
#include <list>
#include <memory>
#include <mutex>

namespace xzero {

/**
 * Implements Scheduler Executor API via native Linux features,
 * such as @c epoll, @c timerfd, @c eventfd, etc.
 */
class XZERO_BASE_API LinuxScheduler : public EventLoop {
 public:
  LinuxScheduler(
      std::unique_ptr<ExceptionHandler> eh,
      std::function<void()> preInvoke,
      std::function<void()> postInvoke);

  explicit LinuxScheduler(
      std::unique_ptr<ExceptionHandler> eh);

  LinuxScheduler();
  ~LinuxScheduler();

  MonotonicTime now() const;

 public:
  using EventLoop::executeOnReadable;
  using EventLoop::executeOnWritable;

  void execute(Task task) override;
  std::string toString() const override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime dt, Task task) override;
  HandleRef executeOnReadable(int fd, Task task, Duration tmo, Task tcb) override;
  HandleRef executeOnWritable(int fd, Task task, Duration tmo, Task tcb) override;
  void cancelFD(int fd) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;

  void runLoop() override;
  void runLoopOnce() override;
  void breakLoop() override;

 private:
  // helper

 private:
  Task onPreInvokePending_;  //!< callback to be invoked before any other hot CB
  Task onPostInvokePending_; //!< callback to be invoked after any other hot CB

  FileDescriptor epollfd_;
  FileDescriptor eventfd_;
  FileDescriptor timerfd_;
  FileDescriptor signalfd_;

  std::atomic<size_t> readerCount_;
  std::atomic<size_t> writerCount_;
};

// std::string inspect(LinuxScheduler::Mode mode);
// std::string inspect(const LinuxScheduler::Watcher& w);
// std::string inspect(const LinuxScheduler& s);

} // namespace xzero
