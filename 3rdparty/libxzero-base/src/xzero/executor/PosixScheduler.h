// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <xzero/RefPtr.h>
#include <xzero/executor/Scheduler.h>
#include <sys/select.h>
#include <set>
#include <vector>
#include <list>
#include <mutex>

namespace xzero {

class WallClock;

class XZERO_BASE_API PosixScheduler : public Scheduler {
 public:
  PosixScheduler(
      std::function<void(const std::exception&)> errorLogger,
      WallClock* clock,
      std::function<void()> preInvoke,
      std::function<void()> postInvoke);

  explicit PosixScheduler(
      std::function<void(const std::exception&)> errorLogger,
      WallClock* clock);

  PosixScheduler();

  ~PosixScheduler();

  using Scheduler::executeOnReadable;
  using Scheduler::executeOnWritable;

  void execute(Task task) override;
  std::string toString() const override;
  HandleRef executeAfter(TimeSpan delay, Task task) override;
  HandleRef executeAt(DateTime dt, Task task) override;
  HandleRef executeOnReadable(int fd, Task task, TimeSpan tmo, Task tcb) override;
  HandleRef executeOnWritable(int fd, Task task, TimeSpan tmo, Task tcb) override;
  void executeOnWakeup(Task task, Wakeup* wakeup, long generation) override;
  size_t timerCount() override;
  size_t readerCount() override;
  size_t writerCount() override;
  size_t taskCount() override;
  void runLoop() override;
  void runLoopOnce() override;
  void breakLoop() override;

  /**
   * Waits at most @p timeout for @p fd to become readable without blocking.
   */
  static void waitForReadable(int fd, TimeSpan timeout);

  /**
   * Waits until given @p fd becomes readable without blocking.
   */
  static void waitForReadable(int fd);

  /**
   * Waits at most @p timeout for @p fd to become writable without blocking.
   */
  static void waitForWritable(int fd, TimeSpan timeout);

  /**
   * Waits until given @p fd becomes writable without blocking.
   */
  static void waitForWritable(int fd);

 public:
  enum class Mode { READABLE, WRITABLE };
  struct Watcher : public Handle { // {{{
    int fd;
    Mode mode;
    Task onIO;
    DateTime timeout;
    Task onTimeout;

    Watcher* prev; //!< predecessor by timeout ASC
    Watcher* next; //!< successor by timeout ASC

    Watcher()
        : Watcher(-1, Mode::READABLE, nullptr, DateTime(0.0), nullptr) {}

    Watcher(const Watcher& w)
        : Watcher(w.fd, w.mode, w.onIO, w.timeout, w.onTimeout) {}

    Watcher(int _fd, Mode _mode, Task _onIO,
            DateTime _timeout, Task _onTimeout)
        : fd(_fd), mode(_mode), onIO(_onIO),
          timeout(_timeout), onTimeout(_onTimeout),
          prev(nullptr), next(nullptr) {
      // Manually ref because we're not holding it in a
      // RefPtr<Watcher> vector in PosixScheduler.
      // - Though, no need to manually unref() either.
      ref();
    }

    void reset(int _fd, Mode _mode, Task _onIO,
            DateTime _timeout, Task _onTimeout) {
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
      timeout = DateTime(0.0);
      prev = nullptr;
      next = nullptr;
    }

    bool operator<(const Watcher& other) const {
      return timeout < other.timeout;
    }
  }; // }}}
  struct Timer : public Handle { // {{{
    DateTime when;
    Task action;

    Timer() : Handle(), when(), action() {}
    Timer(DateTime dt, Task t) : Handle(), when(dt), action(t) {}
    Timer(DateTime dt, Task t, Task c) : Handle(c), when(dt), action(t) {}
    Timer(const Timer&) = default;
    Timer& operator=(const Timer&) = default;
  }; // }}}

 protected:
  /**
   * Adds given timer-handle to the timer-list.
   *
   * @param dt timestamp at which given timer is to be fired.
   * @param task task to invoke upon fire.
   *
   * @note The caller must protect the access itself.
   */
  HandleRef insertIntoTimersList(DateTime dt, Task task);

  void collectTimeouts(std::list<Task>* result);

  void collectActiveHandles(const fd_set* input,
                            const fd_set* output,
                            std::list<Task>* result);

  /**
   * Registers an I/O interest.
   *
   * @note requires the caller to lock the object mutex.
   */
  HandleRef setupWatcher(int fd, Mode mode, Task onFire,
                         TimeSpan timeout, Task onTimeout);

  /**
   * Inserts watcher between @p pred and @p pred's successor.
   *
   * @note requires the caller to lock the object mutex.
   */
  void linkWatcher(Watcher* w, Watcher* pred);

  /**
   * Removes given watcher from ordered list of watchers.
   *
   * @return watcher next to the given watcher, ascending ordered by time.
   * @note requires the caller to lock the object mutex.
   */
  Watcher* unlinkWatcher(Watcher* w);

  /**
   * Retrieves a watcher by file descriptor.
   *
   * @note requires the caller to lock the object mutex.
   */
  Watcher* getWatcher(int fd);

  /**
   * Computes the timespan the event loop should wait the most.
   *
   * @note requires the caller to lock the object mutex.
   */
  TimeSpan nextTimeout() const;

  std::string inspectImpl() const;

  friend std::string inspect(const PosixScheduler&);

 private:
  WallClock* clock_;
  std::mutex lock_;
  int wakeupPipe_[2];

  Task onPreInvokePending_;  //!< callback to be invoked before any other hot CB
  Task onPostInvokePending_; //!< callback to be invoked after any other hot CB

  std::list<Task> tasks_;            //!< list of pending tasks
  std::list<RefPtr<Timer>> timers_;  //!< ASC-sorted list of timers

  std::mutex watcherMutex_;
  std::vector<Watcher> watchers_;   //!< I/O watchers
  Watcher* firstWatcher_;           //!< I/O watcher with the smallest timeout
  Watcher* lastWatcher_;            //!< I/O watcher with the largest timeout

  std::atomic<size_t> readerCount_;
  std::atomic<size_t> writerCount_;
};

XZERO_BASE_API std::string inspect(PosixScheduler::Mode mode);
XZERO_BASE_API std::string inspect(const PosixScheduler::Watcher& w);
XZERO_BASE_API std::string inspect(const PosixScheduler& s);

} // namespace xzero
