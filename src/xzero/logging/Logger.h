// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _libxzero_UTIL_LOGGER_H
#define _libxzero_UTIL_LOGGER_H
#include <atomic>
#include <xzero/logging/LogLevel.h>
#include <xzero/logging/LogTarget.h>

#ifndef STX_LOGGER_MAX_LISTENERS
#define STX_LOGGER_MAX_LISTENERS 64
#endif

namespace xzero {

class Logger {
public:
  Logger();
  static Logger* get();

  void log(
      LogLevel log_level,
      const std::string& component,
      const std::string& message);

  template <typename... T>
  void log(
      LogLevel log_level,
      const std::string& component,
      const std::string& message,
      T... args);

  void logException(
      LogLevel log_level,
      const std::string& component,
      const std::exception& exception,
      const std::string& message);

  template <typename... T>
  void logException(
      LogLevel log_level,
      const std::string& component,
      const std::exception& exception,
      const std::string& message,
      T... args);

  void addTarget(LogTarget* target);
  void setMinimumLogLevel(LogLevel min_level);
  LogLevel getMinimumLogLevel() { return min_level_.load(); }

protected:
  std::atomic<LogLevel> min_level_;
  std::atomic<size_t> max_listener_index_;
  std::atomic<LogTarget*> listeners_[STX_LOGGER_MAX_LISTENERS];
};

} // namespace xzero

#include <xzero/logging/Logger_impl.h>
#endif
