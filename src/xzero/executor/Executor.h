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
#include <xzero/sysconfig.h>

#include <mutex>
#include <atomic>
#include <exception>
#include <functional>
#include <string>

namespace xzero {

/**
 * Closure Execution Service API.
 *
 * Defines an interface for executing tasks. The implementer
 * can distinguish in different execution models, such as threading,
 * sequential or inline execution.
 *
 * @see DirectExecutor
 * @see ThreadPool
 */
class Executor {
 public:
  typedef std::function<void()> Task;

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

 protected:
  void safeCall(std::function<void()> callee) noexcept;

 protected:
  SafeCall safeCall_;
};

} // namespace xzero

#include <xzero/executor/Executor-inl.h>
