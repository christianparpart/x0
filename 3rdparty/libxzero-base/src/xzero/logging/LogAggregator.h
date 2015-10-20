// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
