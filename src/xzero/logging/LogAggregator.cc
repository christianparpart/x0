// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/logging/LogAggregator.h>
#include <xzero/logging/LogSource.h>
#include <xzero/logging/LogTarget.h>
#include <xzero/logging.h>
#include <xzero/Buffer.h>
#include <stdlib.h>

namespace xzero {

LogAggregator::LogAggregator()
    : LogAggregator(LogLevel::Error, LogTarget::console()) {
}

LogAggregator::LogAggregator(LogLevel logLevel, LogTarget* logTarget)
    : target_(logTarget),
      logLevel_(logLevel),
      enabledSources_(),
      activeSources_() {
}

LogAggregator::~LogAggregator() {
  for (auto& activeSource: activeSources_)
    delete activeSource.second;

  activeSources_.clear();
}

void LogAggregator::setLogTarget(LogTarget* target) {
  target_ = target;
}

void LogAggregator::registerSource(LogSource* source) {
  std::lock_guard<std::mutex> _guard(mutex_);
  activeSources_[source->componentName()] = source;
}

void LogAggregator::unregisterSource(LogSource* source) {
  std::lock_guard<std::mutex> _guard(mutex_);
  auto i = activeSources_.find(source->componentName());
  activeSources_.erase(i);
}

LogSource* LogAggregator::getSource(const std::string& componentName) {
  std::lock_guard<std::mutex> _guard(mutex_);

  auto i = activeSources_.find(componentName);

  if (i != activeSources_.end())
    return i->second;
  else {
    LogSource* source = new LogSource(componentName);
    activeSources_[componentName] = source;
    return source;
  }
}

LogAggregator& LogAggregator::get() {
  static LogAggregator aggregator;
  return aggregator;
}

XZERO_INIT static void initializeLogAggregator() {
  try {
    if (const char* target = getenv("XZERO_LOGTARGET")) {
      if (iequals(target, "console")) {
        LogAggregator::get().setLogTarget(LogTarget::console());
      } else if (iequals(target, "syslog")) {
        LogAggregator::get().setLogTarget(LogTarget::syslog());
      } else {
        logError("logging", "Unknown log target \"%s\"", target);
      }
    }

    if (const char* level = getenv("XZERO_LOGLEVEL")) {
      LogAggregator::get().setLogLevel(to_loglevel(level));
    }
  } catch (...) {
    // eat my lunch pack
  }
}

} // namespace xzero
