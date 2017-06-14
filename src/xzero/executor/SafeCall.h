// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/ExceptionHandler.h>

#include <memory>
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
class SafeCall {
 public:
  SafeCall();

  explicit SafeCall(std::unique_ptr<ExceptionHandler> eh);

  /**
   * Configures exception handler.
   */
  void setExceptionHandler(std::unique_ptr<ExceptionHandler> eh);

  /**
   * Safely invokes given task within the callers context.
   *
   * A call to this function will never leak an unhandled exception.
   *
   * @see setExceptionHandler(std::function<void(const std::exception&)>)
   */
  void invoke(std::function<void()> callee) noexcept;

  /**
   * Convinience call operator.
   *
   * @see void invoke(std::function<void()> callee)
   */
  void operator()(std::function<void()> callee) noexcept {
    invoke(callee);
  }

 protected:
  /**
   * Handles uncaught exception.
   */
  void handleException(const std::exception& e) noexcept;

 private:
  std::unique_ptr<ExceptionHandler> exceptionHandler_;
};

} // namespace xzero
