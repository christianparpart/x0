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
#include <unordered_map>

namespace xzero {

/**
 * Implements EventLoop API via native Linux features,
 * such as @c epoll, @c eventfd, @c timerfd, @p signalfd, etc.
 */
class LinuxScheduler : public EventLoop {
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
  enum class Mode { READABLE, WRITABLE };

  struct Watcher : public Executor::Handle {
    Watcher();
    Watcher(const Watcher&);
    Watcher(int fd, Mode mode, Task onIO,
            MonotonicTime timeout, Task onTimeout);

    void clear();
    void reset(int fd, Mode mode, Task onIO,
               MonotonicTime timeout, Task onTimeout);

    int fd;
    Mode mode;
    MonotonicTime timeout;
    Task onTimeout;

    Watcher* prev;
    Watcher* next;
  };

  /**
   * Wakes up loop if currently waiting for events without breaking out.
   */
  void wakeupLoop();

 private:
  /**
   * Registers an I/O interest.
   *
   * @note requires the caller to lock the object mutex.
   */
  HandleRef createWatcher(Mode mode, int fd, Task task, Duration tmo, Task tcb);

  /**
   * Searches for the natural predecessor of given Watcher @p p.
   *
   * @param p watcher to naturally order into the already existing linked list.
   *
   * @returns @c nullptr if it must be put in front, or pointer to the Watcher
   *          element that would directly lead to Watcher @p w.
   */
  Watcher* findPrecedingWatcher(Watcher* w);

  /**
   * Inserts watcher between @p pred and @p pred's successor.
   *
   * @param w     Watcher to inject into the linked list of watchers.
   * @param pred  Predecessor. The watdher @p w will be placed right next to it.
   *              With @p pred being @c nullptr the @p w will be put in front
   *              of all watchers.
   * @return the linked Watcher @p w.
   *
   * @note requires the caller to lock the object mutex.
   */
  Watcher* linkWatcher(Watcher* w, Watcher* pred);

  /**
   * Removes given watcher from ordered list of watchers.
   *
   * @return watcher next to the given watcher, ascending ordered by time.
   * @note requires the caller to lock the object mutex.
   */
  Watcher* unlinkWatcher(Watcher* watcher);

 private:
  Task onPreInvokePending_;  //!< callback to be invoked before any other hot CB
  Task onPostInvokePending_; //!< callback to be invoked after any other hot CB

  std::mutex lock_; //!< mutex, to protect access to tasks, timers

  std::unordered_map<int, Watcher> watchers_;  //!< I/O watchers
  Watcher* firstWatcher_;                      //!< I/O watcher with the smallest timeout
  Watcher* lastWatcher_;                       //!< I/O watcher with the largest timeout

  FileDescriptor epollfd_;
  FileDescriptor eventfd_;
  FileDescriptor timerfd_;
  FileDescriptor signalfd_;

  std::atomic<size_t> readerCount_;
  std::atomic<size_t> writerCount_;
};

LinuxScheduler::Watcher::Watcher(int _fd, Mode _mode, Task _onIO,
                                 MonotonicTime _timeout, Task _onTimeout)
  : fd(_fd),
    mode(_mode),
    timeout(_timeout),
    onTimeout(_onTimeout),
    prev(nullptr),
    next(nullptr) {
}

// std::string inspect(LinuxScheduler::Mode mode);
// std::string inspect(const LinuxScheduler::Watcher& w);
// std::string inspect(const LinuxScheduler& s);

} // namespace xzero
