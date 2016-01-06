// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/logging/LogSource.h>
#include <xzero/logging/LogTarget.h>
#include <xzero/logging/LogAggregator.h>
#include <xzero/RuntimeError.h>
#include <xzero/StackTrace.h>
#include <typeinfo>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdarg.h>

namespace xzero {

LogSource::LogSource(const std::string& componentName)
    : componentName_(componentName) {
}

LogSource::~LogSource() {
}

#define LOG_SOURCE_MSG(level, fmt) \
  if (static_cast<int>(LogLevel::level) <=                                     \
      static_cast<int>(LogAggregator::get().logLevel())) {                     \
    if (LogTarget* target = LogAggregator::get().logTarget()) {                \
      char msg[512];                                                           \
      int n = snprintf(msg, sizeof(msg), "[%s] ", componentName_.c_str());     \
      va_list va;                                                              \
      va_start(va, fmt);                                                       \
      vsnprintf(msg + n, sizeof(msg) - n, fmt, va);                            \
      va_end(va);                                                              \
      target->level(msg);                                                      \
    }                                                                          \
  }

void LogSource::trace(const char* fmt, ...) {
  LOG_SOURCE_MSG(trace, fmt);
}

void LogSource::error(const std::exception& e) {
  if (LogTarget* target = LogAggregator::get().logTarget()) {
    std::stringstream sstr;

    if (auto rt = dynamic_cast<const RuntimeError*>(&e)) {
      auto bt = rt->backtrace();

      sstr << "Exception of type " << typeid(*rt).name() << " caught from "
           << rt->sourceFile() << ":" << rt->sourceLine() << ". "
           << rt->what();

      for (size_t i = 0; i < bt.size(); ++i) {
        sstr << std::endl << "  [" << i << "] " << bt[i];
      }
    } else {
      sstr << "Exception of type "
           << StackTrace::demangleSymbol(typeid(e).name())
           << " caught. " << e.what();
    }

    target->error(sstr.str());
  }
}

void LogSource::debug(const char* fmt, ...) {
  LOG_SOURCE_MSG(debug, fmt);
}

void LogSource::info(const char* fmt, ...) {
  LOG_SOURCE_MSG(info, fmt);
}

void LogSource::warn(const char* fmt, ...) {
  LOG_SOURCE_MSG(warn, fmt);
}

void LogSource::notice(const char* fmt, ...) {
  LOG_SOURCE_MSG(notice, fmt);
}

void LogSource::error(const char* fmt, ...) {
  LOG_SOURCE_MSG(error, fmt);
}

void LogSource::enable() {
  enabled_ = true;
}

bool LogSource::isEnabled() const XZERO_NOEXCEPT {
  return enabled_;
}

void LogSource::disable() {
  enabled_ = false;
}

}  // namespace xzero
