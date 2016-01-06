// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/RefPtr.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/executor/Scheduler.h>
#include <sys/select.h>
#include <set>
#include <vector>
#include <list>
#include <mutex>

namespace xzero {

/**
 * Implements Scheduler Executor API via native Linux features,
 * such as @c epoll, @c timerfd, @c eventfd, etc.
 */
class XZERO_BASE_API LinuxScheduler : public Scheduler {
 public:
  LinuxScheduler(
      std::function<void(const std::exception&)> errorLogger,
      std::function<void()> preInvoke,
      std::function<void()> postInvoke);

  explicit LinuxScheduler(
      std::function<void(const std::exception&)> errorLogger);

  LinuxScheduler();

  ~LinuxScheduler();

  MonotonicTime now() const;

  using Scheduler::executeOnReadable;
  using Scheduler::executeOnWritable;

  void execute(Task task) override;
  std::string toString() const override;
  HandleRef executeAfter(Duration delay, Task task) override;
  HandleRef executeAt(UnixTime dt, Task task) override;
  HandleRef executeOnReadable(int fd, Task task, Duration tmo, Task tcb) override;
  HandleRef executeOnWritable(int fd, Task task, Duration tmo, Task tcb) override;
  HandleRef executeOnSignal(int signo, SignalHandler task) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;
  size_t timerCount() override;
  size_t readerCount() override;
  size_t writerCount() override;
  size_t taskCount() override;
  void runLoop() override;
  void runLoopOnce() override;
  void breakLoop() override;

 protected:
  enum class Mode { READABLE, WRITABLE };
  struct Watcher : public Handle { // {{{
    int fd;
    Mode mode;
    Task onIO;
    UnixTime timeout;
    Task onTimeout;

    Watcher* prev; //!< predecessor by timeout ASC
    Watcher* next; //!< successor by timeout ASC

    Watcher()
        : Watcher(-1, Mode::READABLE, nullptr, UnixTime(0.0), nullptr) {}

    Watcher(const Watcher& w)
        : Watcher(w.fd, w.mode, w.onIO, w.timeout, w.onTimeout) {}

    Watcher(int _fd, Mode _mode, Task _onIO,
            UnixTime _timeout, Task _onTimeout)
        : fd(_fd), mode(_mode), onIO(_onIO),
          timeout(_timeout), onTimeout(_onTimeout),
          prev(nullptr), next(nullptr) {
      // Manually ref because we're not holding it in a
      // RefPtr<Watcher> vector in LinuxScheduler.
      // - Though, no need to manually unref() either.
      ref();
    }

    void reset(int _fd, Mode _mode, Task _onIO,
            UnixTime _timeout, Task _onTimeout) {
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
      timeout = UnixTime(0.0);
      prev = nullptr;
      next = nullptr;
    }

    bool operator<(const Watcher& other) const {
      return timeout < other.timeout;
    }
  }; // }}}
  struct Timer : public Handle { // {{{
    UnixTime when;
    Task action;

    Timer() : Handle(), when(), action() {}
    Timer(UnixTime dt, Task t) : Handle(), when(dt), action(t) {}
    Timer(UnixTime dt, Task t, Task c) : Handle(c), when(dt), action(t) {}
    Timer(const Timer&) = default;
    Timer& operator=(const Timer&) = default;
  }; // }}}

 private:
  Watcher* linkWatcher(Watcher* w, Watcher* pred);
  Watcher* unlinkWatcher(Watcher* w);
  HandleRef insertTimer(MonotonicTime time, Task task);

  void collectTimeouts(std::list<Task>* result);

 private:
  Task onPreInvokePending_;  //!< callback to be invoked before any other hot CB
  Task onPostInvokePending_; //!< callback to be invoked after any other hot CB

  FileDescriptor epollfd_;
  FileDescriptor eventfd_;
  FileDescriptor signalfd_;

  std::atomic<size_t> readerCount_;
  std::atomic<size_t> writerCount_;
};

XZERO_BASE_API std::string inspect(LinuxScheduler::Mode mode);
XZERO_BASE_API std::string inspect(const LinuxScheduler::Watcher& w);
XZERO_BASE_API std::string inspect(const LinuxScheduler& s);

} // namespace xzero
