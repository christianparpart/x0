// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/sysconfig.h>
#include <xzero/logging/LogLevel.h>
#include <string>
#include <mutex>
#include <unordered_map>

namespace xzero {

class LogSource;
class LogTarget;

/**
 * Logging Aggregator Service.
 */
class XZERO_BASE_API LogAggregator {
 public:
  LogAggregator();
  LogAggregator(LogLevel logLevel, LogTarget* logTarget);
  ~LogAggregator();

  LogLevel logLevel() const XZERO_NOEXCEPT { return logLevel_; }
  void setLogLevel(LogLevel level) { logLevel_ = level; }

  LogTarget* logTarget() const XZERO_NOEXCEPT { return target_; }
  void setLogTarget(LogTarget* target);

  void registerSource(LogSource* source);
  void unregisterSource(LogSource* source);
  LogSource* getSource(const std::string& className);

  static LogAggregator& get();

 private:
  LogTarget* target_;
  LogLevel logLevel_;
  std::mutex mutex_;
  std::unordered_map<std::string, bool> enabledSources_;
  std::unordered_map<std::string, LogSource*> activeSources_;
};

}  // namespace xzero
