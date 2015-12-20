// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/executor/SafeCall.h>
#include <xzero/ExceptionHandler.h>
#include <xzero/RefCounted.h>
#include <xzero/RefPtr.h>
#include <xzero/Duration.h>
#include <xzero/UnixTime.h>
#include <xzero/sysconfig.h>

#include <memory>
#include <mutex>
#include <atomic>
#include <exception>
#include <functional>
#include <string>

namespace xzero {

class Wakeup;
class UnixSignals;
class UnixSignalInfo;

/**
 * Closure Execution Service API.
 *
 * Defines an interface for executing tasks. The implementer
 * can distinguish in different execution models, such as threading,
 * sequential or inline execution.
 *
 * @see LocalExecutor
 * @see ThreadPool
 */
class Executor {
 public:
  typedef std::function<void()> Task;
  typedef std::function<void(const UnixSignalInfo&)> SignalHandler;

  struct Handle : public RefCounted { // {{{
   public:
    Handle()
        : Handle(nullptr) {}

    explicit Handle(Task onCancel)
        : mutex_(),
          isCancelled_(false),
          onCancel_(onCancel) {}

    bool isCancelled() const;

    void cancel();
    void fire(Task task);

    void reset(Task onCancel);

    void setCancelHandler(Task task);

   private:
    std::mutex mutex_;
    std::atomic<bool> isCancelled_;
    Task onCancel_;
  }; // }}}
  typedef RefPtr<Handle> HandleRef;

  explicit Executor(std::unique_ptr<xzero::ExceptionHandler> eh);

  virtual ~Executor();

  void setExceptionHandler(std::unique_ptr<ExceptionHandler> eh);

  /**
   * Retrieves a human readable name of this executor (for introspection only).
   */
  virtual std::string toString() const = 0;

  /**
   * Executes given task.
   */
  virtual void execute(Task task) = 0;

  /**
   * Runs given task when given selectable is non-blocking readable.
   *
   * @param fd file descriptor to watch for non-blocking readability.
   * @param task Task to execute upon given event.
   * @param timeout Duration to wait for readability.
   *                When this timeout is hit and no readable-event was
   *                generated yet, the @p onTimeout task will be invoked
   *                instead and the selectable will no longer be watched on.
   */
  virtual HandleRef executeOnReadable(
      int fd, Task task,
      Duration timeout, Task onTimeout) = 0;

  /**
   * Runs given task when given selectable is non-blocking writable.
   *
   * @param fd file descriptor to watch for non-blocking readability.
   * @param task Task to execute upon given event.
   * @param timeout timeout to wait for readability. When the timeout is hit
   *                and no readable-event was generated yet, an
   *                the task isstill fired but fd will raise with ETIMEDOUT.
   */
  virtual HandleRef executeOnWritable(
      int fd, Task task,
      Duration timeout, Task onTimeout) = 0;

  virtual void cancelFD(int fd) = 0;

  /**
   * Runs given task when given signal was received.
   *
   * @param signo UNIX signal number (such as SIGTERM) you want to be called * for.
   * @param task  The SignalHandler to execute upon given event.
   *
   * @note If you always want to get notified on a given signal, you must
   *       reregister yourself each time you have been fired.
   */
  virtual HandleRef executeOnSignal(int signo, SignalHandler task);

  /**
   * Runs given task when given selectable is non-blocking readable.
   *
   * @param fd file descriptor to watch for non-blocking readability.
   * @param task Task to execute upon given event.
   */
  HandleRef executeOnReadable(int fd, Task task);

  /**
   * Runs given task when given selectable is non-blocking writable.
   *
   * @param fd file descriptor to watch for non-blocking readability.
   * @param task Task to execute upon given event.
   */
  HandleRef executeOnWritable(int fd, Task task);

  /**
   * Schedules given task to be run after given delay.
   *
   * @param task the actual task to be executed.
   * @param delay the timespan to wait before actually executing the task.
   */
  virtual HandleRef executeAfter(Duration delay, Task task) = 0;

  /**
   * Runs given task at given time.
   */
  virtual HandleRef executeAt(UnixTime ts, Task task) = 0;

  /**
   * Executes @p task when given @p wakeup triggered a wakeup event
   * for >= @p generation.
   *
   * @param task Task to invoke when the wakeup is triggered.
   * @param wakeup Wakeup object to watch
   * @param generation Generation number to match at least.
   */
  virtual void executeOnWakeup(Task task, Wakeup* wakeup, long generation) = 0;

  /**
   * Run the provided task when the wakeup handle is woken up.
   */
  void executeOnNextWakeup(std::function<void()> task, Wakeup* wakeup);

  /**
   * Run the provided task when the wakeup handle is woken up.
   */
  void executeOnFirstWakeup(std::function<void()> task, Wakeup* wakeup);

  /**
   * Retrieves the number of references to tasks that are waiting to be invoked.
   *
   * In the Executor API, the reference count is incremented on each
   * task yet to be executed and decremented once executed.
   *
   * The reference count is the number of tasks still pending which helps
   * runLoop() to determine wheather to continue iterating over
   * runLoopOnce() or to return to its caller.
   *
   * Sometimes you need to manually decrement the reference count in order
   * to explicitely ignore a pending task in the loop.
   *
   * However, you must increment the reference manually as soon
   * as your task has either been invoked or is to be cancelled to not
   * cause bugs in counting.
   */
  int referenceCount() const noexcept;

  /**
   * Increments the reference count.
   *
   * @see referenceCount() const noexcept;
   */
  void ref();

  /**
   * Decrements the reference count by 1.
   *
   * @see referenceCount() const noexcept;
   */
  void unref();

  /**
   * Decrements the reference count by @p count.
   *
   * @see referenceCount() const noexcept;
   */
  void unref(int count);

 protected:
  void safeCall(std::function<void()> callee) noexcept;

 protected:
  SafeCall safeCall_;
  std::unique_ptr<UnixSignals> unixSignals_;
  std::atomic<int> refs_;
};

} // namespace xzero

#include <xzero/executor/Executor-inl.h>
