// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
    : SafeCall(std::unique_ptr<ExceptionHandler>(new CatchAndLogExceptionHandler("SafeCall"))) {
}

SafeCall::SafeCall(std::unique_ptr<ExceptionHandler> eh)
    : exceptionHandler_(std::move(eh)) {
}

void SafeCall::setExceptionHandler(std::unique_ptr<ExceptionHandler> eh) {
  exceptionHandler_ = std::move(eh);
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
      exceptionHandler_->onException(e);
    } catch (...) {
    }
  }
}

} // namespace xzero
