// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/sysconfig.h>
#include <string>

namespace std {
  class exception;
}

namespace xzero {

/**
 * A logging source.
 *
 * Creates a log message such as "[$ClassName] $LogMessage"
 *
 * Your class that needs logging creates a static member of LogSource
 * for generating logging messages.
 */
class XZERO_BASE_API LogSource {
 public:
  explicit LogSource(const std::string& component);
  ~LogSource();

  void trace(const char* fmt, ...);
  void debug(const char* fmt, ...);
  void info(const char* fmt, ...);
  void warn(const char* fmt, ...);
  void notice(const char* fmt, ...);
  void error(const char* fmt, ...);
  void error(const std::exception& e);

  void enable();
  bool isEnabled() const XZERO_NOEXCEPT;
  void disable();

  const std::string& componentName() const XZERO_NOEXCEPT { return componentName_; }

 private:
  std::string componentName_;
  bool enabled_;
};

}  // namespace xzero
