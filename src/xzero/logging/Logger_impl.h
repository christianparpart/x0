// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _libxzero_UTIL_LOGGER_IMPL_H
#define _libxzero_UTIL_LOGGER_IMPL_H
#include <xzero/StringUtil.h>

namespace xzero {

template <typename... T>
void Logger::log(
    LogLevel log_level,
    const std::string& component,
    const std::string& message,
    T... args) {
  if (log_level >= min_level_) {
    log(log_level, component, StringUtil::format(message, args...));
  }
}

template <typename... T>
void Logger::logException(
    LogLevel log_level,
    const std::string& component,
    const std::exception& exception,
    const std::string& message,
    T... args) {
  if (log_level >= min_level_) {
    logException(
        log_level,
        component,
        exception,
        StringUtil::format(message, args...));
  }
}

} // namespace xzero

#endif
