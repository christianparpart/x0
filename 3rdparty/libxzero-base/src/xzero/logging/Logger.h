/**
 * This file is part of the "libxzero" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * libxzero is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef _libxzero_UTIL_LOGGER_H
#define _libxzero_UTIL_LOGGER_H
#include <atomic>
#include <xzero/UnixTime.h>
#include <xzero/stdtypes.h>
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
      const String& component,
      const String& message);

  template <typename... T>
  void log(
      LogLevel log_level,
      const String& component,
      const String& message,
      T... args);

  void logException(
      LogLevel log_level,
      const String& component,
      const std::exception& exception,
      const String& message);

  template <typename... T>
  void logException(
      LogLevel log_level,
      const String& component,
      const std::exception& exception,
      const String& message,
      T... args);

  void addTarget(LogTarget* target);
  void setMinimumLogLevel(LogLevel min_level);

protected:
  std::atomic<LogLevel> min_level_;
  std::atomic<size_t> max_listener_index_;
  std::atomic<LogTarget*> listeners_[STX_LOGGER_MAX_LISTENERS];
};

} // namespace xzero

#include <xzero/logging/Logger_impl.h>
#endif
