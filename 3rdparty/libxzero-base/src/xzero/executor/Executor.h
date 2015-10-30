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
#include <xzero/sysconfig.h>

#include <exception>
#include <deque>
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
  explicit Executor(std::unique_ptr<xzero::ExceptionHandler> eh);

  virtual ~Executor();

  typedef std::function<void()> Task;

  /**
   * Executes given task.
   */
  virtual void execute(Task task) = 0;

  /**
   * Retrieves a human readable name of this executor (for introspection only).
   */
  virtual std::string toString() const = 0;

  void setExceptionHandler(std::unique_ptr<ExceptionHandler> eh);

 protected:
  void safeCall(std::function<void()> callee) noexcept;

 protected:
  SafeCall safeCall_;
};

} // namespace xzero
