// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _libxzero_UTIL_LOGGING_H
#define _libxzero_UTIL_LOGGING_H

#include <xzero/logging/LogLevel.h>
#include <xzero/logging/Logger.h>

namespace xzero {

/**
 * EMERGENCY: Something very bad happened
 */
template <typename... T>
void logEmergency(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kEmergency, component, msg, args...);
}

template <typename... T>
void logEmergency(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kEmergency, component, e, msg, args...);
}

/**
 * ALERT: Action must be taken immediately
 */
template <typename... T>
void logAlert(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kAlert, component, msg, args...);
}

template <typename... T>
void logAlert(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kAlert, component, e, msg, args...);
}

/**
 * CRITICAL: Action should be taken as soon as possible
 */
template <typename... T>
void logCritical(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kCritical, component, msg, args...);
}

template <typename... T>
void logCritical(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kCritical, component, e, msg, args...);
}

/**
 * ERROR: User-visible Runtime Errors
 */
template <typename... T>
void logError(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kError, component, msg, args...);
}

template <typename... T>
void logError(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kError, component, e, msg, args...);
}

/**
 * WARNING: Something unexpected happened that should not have happened
 */
template <typename... T>
void logWarning(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kWarning, component, msg, args...);
}

template <typename... T>
void logWarning(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kWarning, component, e, msg, args...);
}

/**
 * NOTICE: Normal but significant condition.
 */
template <typename... T>
void logNotice(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kNotice, component, msg, args...);
}

template <typename... T>
void logNotice(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kNotice, component, e, msg, args...);
}

/**
 * INFO: Informational messages
 */
template <typename... T>
void logInfo(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kInfo, component, msg, args...);
}

template <typename... T>
void logInfo(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kInfo, component, e, msg, args...);
}

/**
 * DEBUG: Debug messages
 */
template <typename... T>
void logDebug(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kDebug, component, msg, args...);
}

template <typename... T>
void logDebug(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kDebug, component, e, msg, args...);
}

/**
 * TRACE: Trace messages
 */
template <typename... T>
void logTrace(const std::string& component, const std::string& msg, T... args) {
  Logger::get()->log(LogLevel::kTrace, component, msg, args...);
}

template <typename... T>
void logTrace(
    const std::string& component,
    const std::exception& e,
    const std::string& msg,
    T... args) {
  Logger::get()->logException(LogLevel::kTrace, component, e, msg, args...);
}

} // namespace xzero

#endif
