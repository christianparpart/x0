// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
    const std::string& component,
    const std::exception& exception,
    const std::string& message) {
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
      const std::string& component,
      const std::string& message) {
  if (log_level < min_level_) {
    return;
  }

  const size_t max_idx = max_listener_index_.load();
  for (size_t i = 0; i < max_idx; ++i) {
    auto listener = listeners_[i].load();

    if (listener != nullptr) {
      listener->log(log_level, component, message);
    }
  }
}

void Logger::addTarget(LogTarget* target) {
  // avoid adding it twice
  for (size_t i = 0, e = max_listener_index_.load(); i != e; ++i)
    if (listeners_[i].load() == target)
      return;

  auto listener_id = max_listener_index_.fetch_add(1);
  listeners_[listener_id] = target;
}

void Logger::setMinimumLogLevel(LogLevel min_level) {
  min_level_ = min_level;
}

} // namespace xzero

