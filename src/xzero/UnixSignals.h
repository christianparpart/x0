// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/defines.h>
#include <xzero/executor/Executor.h>
#include <memory>

namespace xzero {

/**
 * UNIX Signal Notification API.
 */
class UnixSignals {
 public:
  typedef Executor::Task Task;
  typedef Executor::HandleRef HandleRef;

  virtual ~UnixSignals() {}

  /**
   * Creates a platform-dependant instance of UnixSignals.
   */
  static std::unique_ptr<UnixSignals> create(Executor* executor);

  /**
   * Runs given @p task when given @p signal was received.
   *
   * @param signo UNIX signal number (such as @c SIGTERM) you want
   *              to be called for.
   * @param task  Task to execute upon given event.
   *
   * @note If you always want to get notified on a given signal, you must
   *       reregister yourself each time you have been fired.
   *
   * @see Executor::executeOnSignal(int signal, Task task)
   */
  virtual HandleRef executeOnSignal(int signal, Task task) = 0;
};

} // namespace xzero
