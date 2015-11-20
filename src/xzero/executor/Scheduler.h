// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/executor/Executor.h>
#include <functional>
#include <memory>

namespace xzero {

/**
 * Interface for scheduling tasks.
 */
class Scheduler : public Executor {
 public:
  Scheduler(std::unique_ptr<ExceptionHandler> eh)
      : Executor(std::move(eh)) {}

  /**
   * Retrieves the number of active timers.
   *
   * @see executeAt(UnixTime dt, Task task)
   * @see executeAfter(Duration ts, Task task)
   */
  virtual size_t timerCount() = 0;

  /**
   * Retrieves the number of active read-interests.
   *
   * @see executeOnReadable(int fd, Task task)
   */
  virtual size_t readerCount() = 0;

  /**
   * Retrieves the number of active write-interests.
   *
   * @see executeOnWritable(int fd, Task task)
   */
  virtual size_t writerCount() = 0;

  /**
   * Retrieves the number of pending tasks.
   *
   * @see execute(Task task)
   */
  virtual size_t taskCount() = 0;

  /**
   * Runs the event loop until no event is to be served.
   */
  virtual void runLoop() = 0;

  /**
   * Runs the event loop exactly once, possibly blocking until an event is
   * fired..
   */
  virtual void runLoopOnce() = 0;

  /**
   * Breaks loop in case it is blocking while waiting for an event.
   */
  virtual void breakLoop() = 0;

 protected:
  template<typename Container>
  void safeCallEach(const Container& tasks) {
    for (Task task: tasks) {
      safeCall(task);
    }
  }
};

}  // namespace xzero
