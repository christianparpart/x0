// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _libxzero_UTIL_EXCEPTIONHANDLER_H
#define _libxzero_UTIL_EXCEPTIONHANDLER_H
#include <mutex>
#include <xzero/stdtypes.h>

namespace xzero {

class ExceptionHandler {
public:
  virtual ~ExceptionHandler() {}
  virtual void onException(const std::exception& error) const = 0;

  void operator()(const std::exception& e) const { onException(e); }
};

class CatchAndLogExceptionHandler : public ExceptionHandler {
public:
  CatchAndLogExceptionHandler(const String& component);
  void onException(const std::exception& error) const override;
protected:
  String component_;
};

class CatchAndAbortExceptionHandler : public ExceptionHandler {
public:
  CatchAndAbortExceptionHandler(const std::string& message = "Aborting...");
  void onException(const std::exception& error) const override;

  void installGlobalHandlers();

protected:
  std::string message_;
};

}
#endif
