// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/MonotonicTime.h>
#include <xzero/PosixSignals.h>
#include <xzero/executor/EventLoop.h>
#include <xzero/io/FileDescriptor.h>
#include <vector>
#include <set>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <iosfwd>
#include <sys/epoll.h>

namespace xzero {

/**
 * Wrapper API around the native Linux eventfd feature.
 */
class EventFd {
 public:
  EventFd();

  void notify(uint64_t n = 1);
  std::optional<uint64_t> consume();

  int native() { return handle_; }

 private:
  FileDescriptor handle_;
};

/**
 * Implements EventLoop API via native Linux features,
 * such as @c epoll, @c eventfd, @p signalfd, etc.
 */
class LinuxScheduler : public EventLoop {
 public:
  explicit LinuxScheduler(ExceptionHandler eh);
  LinuxScheduler();
  ~LinuxScheduler();

  MonotonicTime now() const noexcept;
  void updateTime();

 public:
  using EventLoop::executeOnReadable;
  using EventLoop::executeOnWritable;

  void execute(Task task) override;
  std::string toString() const override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime dt, Task task) override;
  HandleRef executeOnSignal(int signo, SignalHandler handler) override;
  HandleRef executeOnReadable(const Socket& s, Task task, Duration tmo, Task tcb) override;
  HandleRef executeOnWritable(const Socket& s, Task task, Duration tmo, Task tcb) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;

  HandleRef executeOnReadable(int fd, Task task, Duration tmo, Task tcb);
  HandleRef executeOnWritable(int fd, Task task, Duration tmo, Task tcb);

  void runLoop() override;
  void runLoopOnce() override;
  void breakLoop() override;

  enum class Mode { READABLE, WRITABLE };

  struct Watcher : public Handle { // {{{
    Watcher();
    Watcher(const Watcher&);
    Watcher(int fd, Mode mode, Task onIO,
            MonotonicTime timeout, Task onTimeout);

    void clear();
    void reset(int fd, Mode mode, Task onIO,
               MonotonicTime timeout, Task onTimeout);

    int fd;
    Mode mode;
    Task onIO;
    MonotonicTime timeout;
    Task onTimeout;

    Watcher* prev;
    Watcher* next;
  }; // }}}
  struct Timer : public Handle { // {{{
    MonotonicTime when;
    Task action;

    Timer() : Handle(), when(), action() {}
    Timer(MonotonicTime dt, Task t) : Handle(), when(dt), action(t) {}
    Timer(MonotonicTime dt, Task t, Task c) : Handle(c), when(dt), action(t) {}
    Timer(const Timer&) = default;
    Timer& operator=(const Timer&) = default;
  }; // }}}

 private:
  inline static int makeEvent(Mode mode);
  void loop(bool repeat);
  size_t waitForEvents() noexcept;
  std::list<Task> collectEvents(size_t count);

  /**
   * Wakes up loop if currently waiting for events without breaking out.
   */
  void wakeupLoop();

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
  HandleRef linkWatcher(Watcher* w, Watcher* pred);

  /**
   * Removes given watcher from ordered list of watchers.
   *
   * @return watcher next to the given watcher, ascending ordered by time.
   * @note requires the caller to lock the object mutex.
   */
  Watcher* unlinkWatcher(Watcher* watcher);

  /**
   * Searches handle for given file descriptor @p fd.
   * @return either the reference to the handle or @c nullptr.
   */
  HandleRef findWatcher(int fd);

  /**
   * Computes the timespan the event loop should wait the most.
   *
   * @note requires the caller to lock the object mutex.
   */
  MonotonicTime nextTimeout() const noexcept;

  void collectActiveHandles(size_t count, std::list<Task>* result);

  HandleRef insertIntoTimersList(MonotonicTime dt, Task task);

  void collectTimeouts(std::list<Task>* result);

  void onSignal();

 private:
  std::mutex lock_; //!< mutex, to protect access to tasks, timers

  std::unordered_map<int, std::shared_ptr<Watcher>> watchers_;  //!< I/O watchers
  Watcher* firstWatcher_;                      //!< I/O watcher with the smallest timeout
  Watcher* lastWatcher_;                       //!< I/O watcher with the largest timeout

  std::list<Task> tasks_;                       //!< list of pending tasks
  std::list<std::shared_ptr<Timer>> timers_;    //!< ASC-sorted list of timers

  FileDescriptor epollfd_;
  EventFd eventfd_;

  std::mutex signalLock_;
  FileDescriptor signalfd_;
  sigset_t signalMask_;
  std::atomic<size_t> signalInterests_;
  class SignalWatcher;
  std::vector<std::list<std::shared_ptr<SignalWatcher>>> signalWatchers_;

  MonotonicTime now_;

  std::vector<epoll_event> activeEvents_;

  std::atomic<size_t> readerCount_;
  std::atomic<size_t> writerCount_;
  std::atomic<size_t> breakLoopCounter_;
};

class LinuxScheduler::SignalWatcher : public Executor::Handle {
 public:
  using Task = Executor::Task;

  explicit SignalWatcher(SignalHandler action)
      : action_(action) {};

  void fire() {
    Executor::Handle::fire(std::bind(action_, info));
  }

  UnixSignalInfo info;

 private:
  SignalHandler action_;
};

} // namespace xzero

#include <xzero/executor/LinuxScheduler-inl.h>
