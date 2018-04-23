// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/executor/SafeCall.h>
#include <xzero/ExceptionHandler.h>
#include <xzero/logging.h>
#include <exception>

namespace xzero {

SafeCall::SafeCall()
    : SafeCall(CatchAndLogExceptionHandler("SafeCall")) {
}

SafeCall::SafeCall(ExceptionHandler eh)
    : exceptionHandler_(eh) {
}

void SafeCall::setExceptionHandler(ExceptionHandler eh) {
  exceptionHandler_ = eh;
}

void SafeCall::invoke(std::function<void()> task) noexcept {
  try {
    if (task) {
      task();
    }
  } catch (const std::exception& e) {
    handleException(e);
  } catch (...) {
  }
}

void SafeCall::handleException(const std::exception& e) noexcept {
  if (exceptionHandler_) {
    try {
      exceptionHandler_(e);
    } catch (...) {
    }
  }
}

} // namespace xzero
