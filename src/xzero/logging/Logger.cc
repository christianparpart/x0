/**
 * This file is part of the "libxzero" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *   Copyright (c) 2009-2015 Christian Parpart.
 *
 * libxzero is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <xzero/logging/Logger.h>
#include <xzero/RuntimeError.h>
#include <xzero/inspect.h>
#include <stdarg.h>
#include <sstream>

namespace xzero {

Logger* Logger::get() {
  static Logger singleton;
  return &singleton;
}

Logger::Logger() :
    min_level_(LogLevel::kNotice),
    max_listener_index_(0) {
  for (int i = 0; i < STX_LOGGER_MAX_LISTENERS; ++i) {
    listeners_[i] = nullptr;
  }
}

void Logger::logException(
    LogLevel log_level,
    const String& component,
    const std::exception& exception,
    const String& message) {
  if (log_level < min_level_) {
    return;
  }

  try {
    auto rte = dynamic_cast<const RuntimeError*>(&exception);
    if (rte != nullptr) {
      log(log_level,
          component,
          "$0: $1: $2\n    in $3\n    in $4:$5",
          message,
          rte->typeName(),
          rte->what(),
          rte->functionName(),
          rte->sourceFile(),
          rte->sourceLine());
    } else {
      log(log_level,
          component,
          "$0: std::exception: <foreign exception> $1",
          message,
          exception.what());
    }
  } catch (const std::exception& bcee) {
    log(
        log_level,
        component,
        "$0: std::exception: <nested exception> $1",
        message,
        exception.what());
  }
}

void Logger::log(
      LogLevel log_level,
      const String& component,
      const String& message) {
  if (log_level < min_level_) {
    return;
  }

  const auto max_idx = max_listener_index_.load();
  for (int i = 0; i < max_idx; ++i) {
    auto listener = listeners_[i].load();

    if (listener != nullptr) {
      listener->log(log_level, component, message);
    }
  }
}

void Logger::addTarget(LogTarget* target) {
  auto listener_id = max_listener_index_.fetch_add(1);
  listeners_[listener_id] = target;
}

void Logger::setMinimumLogLevel(LogLevel min_level) {
  min_level_ = min_level;
}

} // namespace xzero

