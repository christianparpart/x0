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
#include <xzero/sysconfig.h>

#include <exception>
#include <deque>
#include <functional>
#include <string>

namespace xzero {

/**
 * helper class for safely invoking callbacks with regards to unhandled
 * exceptions.
 *
 * Unhandled exceptions will be caught and passed to the exception handler,
 * i.e. to log them.
 */
class XZERO_BASE_API SafeCall {
 public:
  SafeCall();
  explicit SafeCall(std::function<void(const std::exception&)> eh);

  /**
   * Configures exception handler.
   */
  void setExceptionHandler(std::function<void(const std::exception&)> eh);

  /**
   * Savely invokes given task within the callers context.
   *
   * A call to this function will never leak an unhandled exception.
   *
   * @see setExceptionHandler(std::function<void(const std::exception&)>)
   */
  void safeCall(std::function<void()> callee) XZERO_NOEXCEPT;

  /**
   * Convinience call operator.
   *
   * @see void safeCall(std::function<void()> callee)
   */
  void operator()(std::function<void()> callee) XZERO_NOEXCEPT {
    safeCall(callee);
  }

 protected:
  /**
   * Handles uncaught exception.
   */
  void handleException(const std::exception& e) XZERO_NOEXCEPT;

 private:
  std::function<void(const std::exception&)> exceptionHandler_;
};

} // namespace xzero
