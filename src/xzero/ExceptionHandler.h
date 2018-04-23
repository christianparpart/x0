// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <functional>
#include <exception>
#include <string>

namespace xzero {

typedef std::function<void(const std::exception&)> ExceptionHandler;

class CatchAndLogExceptionHandler {
public:
  explicit CatchAndLogExceptionHandler(const std::string& component);
  void onException(const std::exception& error) const;
  void operator()(const std::exception& e) { onException(e); }

protected:
  std::string component_;
};

class CatchAndAbortExceptionHandler {
public:
  explicit CatchAndAbortExceptionHandler(const std::string& message = "Aborting...");
  void onException(const std::exception& error) const;
  void operator()(const std::exception& e) { onException(e); }
  void installGlobalHandlers();

protected:
  std::string message_;
};

} // namespace xzero
