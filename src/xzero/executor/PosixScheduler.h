// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/executor/EventLoop.h>
#include <xzero/PosixSignals.h>
#include <xzero/net/SocketPair.h>
#include <xzero/MonotonicTime.h>
#include <xzero/sysconfig.h>
#include <xzero/defines.h>

#include <tuple>
#include <vector>
#include <list>
#include <mutex>

#if defined(HAVE_SYS_SELECT_H)
#include <sys/select.h>
#endif

#if defined(XZERO_OS_WIN32)
#include <WinSock2.h>
#endif

namespace xzero {

class PosixScheduler : public EventLoop {
 public:
  PosixScheduler(const PosixScheduler&) = delete;
  PosixScheduler& operator=(const PosixScheduler&) = delete;

  PosixScheduler(PosixScheduler&&) = default;
  PosixScheduler& operator=(PosixScheduler&&) = default;

  explicit PosixScheduler(ExceptionHandler eh);

  PosixScheduler();

  ~PosixScheduler();

  MonotonicTime now() const noexcept;

  /**
   * Updates internal time.
   *
   * Call this function when you want to add new timers or timeouts
   * outside of the event loop, for example from a different thread or
   * if you're adding a timer or timeout before the event loop has
   * been entered or if your callbacks just take very long and you
   * need to refresh the internal time before adding a new time relevant
   * interest to the event loop.
   *
   * @see now(), executeAfter(), executeAt()
   * @see executeOnReadable(), executeOnWritable()
   */
  void updateTime();

 public:
  using EventLoop::executeOnReadable;
  using EventLoop::executeOnWritable;

  void execute(Task task) override;
  std::string toString() const override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime dt, Task task) override;
  HandleRef executeOnReadable(const Socket& s, Task task, Duration tmo, Task tcb) override;
  HandleRef executeOnWritable(const Socket& s, Task task, Duration tmo, Task tcb) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;

  void runLoop() override;
  void runLoopOnce() override;
  void breakLoop() override;

  void wakeupLoop();

  /**
   * Waits at most @p timeout for @p fd to become readable without blocking.
   */
  static void waitForReadable(const Socket& s, Duration timeout);

  /**
   * Waits until given @p fd becomes readable without blocking.
   */
  static void waitForReadable(const Socket& s);

  /**
   * Waits at most @p timeout for @p fd to become writable without blocking.
   */
  static void waitForWritable(const Socket& s, Duration timeout);

  /**
   * Waits until given @p fd becomes writable without blocking.
   */
  static void waitForWritable(const Socket& s);

 public:
  enum class Mode { READABLE, WRITABLE };
  struct Watcher : public Handle { // {{{
    int fd;
    Mode mode;
    Task onIO;
    MonotonicTime timeout;
    Task onTimeout;

    Watcher* prev; //!< predecessor by timeout ASC
    Watcher* next; //!< successor by timeout ASC

    Watcher()
        : Watcher(-1, Mode::READABLE, nullptr, MonotonicTime::Zero, nullptr) {}

    Watcher(const Watcher& w)
        : Watcher(w.fd, w.mode, w.onIO, w.timeout, w.onTimeout) {}

    Watcher(int _fd, Mode _mode, Task _onIO,
            MonotonicTime _timeout, Task _onTimeout)
        : fd(_fd), mode(_mode), onIO(_onIO),
          timeout(_timeout), onTimeout(_onTimeout),
          prev(nullptr), next(nullptr) {
    }

    void reset(int _fd, Mode _mode, Task _onIO,
            MonotonicTime _timeout, Task _onTimeout) {
      fd = _fd;
      mode = _mode;
      onIO = _onIO;
      timeout = _timeout;
      onTimeout = _onTimeout;

      prev = nullptr;
      next = nullptr;

      Handle::reset(nullptr);
    }

    void clear() {
      fd = -1;
      timeout = MonotonicTime::Zero;
      prev = nullptr;
      next = nullptr;
    }

    bool operator<(const Watcher& other) const {
      return timeout < other.timeout;
    }
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

 protected:
  int collectWatches();

  /**
   * Adds given timer-handle to the timer-list.
   *
   * @param dt timestamp at which given timer is to be fired.
   * @param task task to invoke upon fire.
   *
   * @note The caller must protect the access itself.
   */
  HandleRef insertIntoTimersList(MonotonicTime dt, Task task);

  void loop(bool repeat);
  size_t waitForEvents() noexcept;
  std::list<EventLoop::Task> collectEvents();
  void collectTimeouts(std::list<Task>* result);
  void collectActiveHandles(std::list<Task>* result);

  /**
   * Registers an I/O interest.
   *
   * @note requires the caller to lock the object mutex.
   */
  HandleRef setupWatcher(int fd, Mode mode, Task onFire,
                         Duration timeout, Task onTimeout);

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
  Watcher* unlinkWatcher(Watcher* w);

  /**
   * Searches handle for given file descriptor @p fd.
   * @return either the reference to the handle or @c nullptr.
   */
  HandleRef findWatcher(int fd);

  friend std::string inspect(const PosixScheduler&);

 private:
  std::mutex lock_;                 //!< mutex, to protect access to tasks, timers

  SocketPair wakeupPipe_;           //!< system pipe, used to wakeup the waiting syscall

  std::list<Task> tasks_;           //!< list of pending tasks
  std::list<std::shared_ptr<Timer>> timers_;  //!< ASC-sorted list of timers

  std::vector<std::shared_ptr<Watcher>> watchers_; //!< I/O watchers
  Watcher* firstWatcher_;           //!< I/O watcher with the smallest timeout
  Watcher* lastWatcher_;            //!< I/O watcher with the largest timeout

  std::atomic<size_t> readerCount_; //!< number of active read interests
  std::atomic<size_t> writerCount_; //!< number of active write interests
  std::atomic<size_t> breakLoopCounter_;

  MonotonicTime now_;
  fd_set input_;
  fd_set output_;
  fd_set error_;
};

std::string inspect(PosixScheduler::Mode mode);
std::string inspect(const PosixScheduler::Watcher& w);
std::string inspect(const PosixScheduler& s);

} // namespace xzero
