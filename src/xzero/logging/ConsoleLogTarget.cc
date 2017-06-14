// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/logging/ConsoleLogTarget.h>
#include <xzero/logging/LogLevel.h>
#include <xzero/logging.h>
#include <xzero/AnsiColor.h>
#include <xzero/WallClock.h>
#include <xzero/UnixTime.h>
#include <unistd.h>

namespace xzero {

ConsoleLogTarget::ConsoleLogTarget()
    : timestampEnabled_(true) {
}

ConsoleLogTarget* ConsoleLogTarget::get() {
  static ConsoleLogTarget singleton;
  return &singleton;
}

// TODO is a mutex required for concurrent printf()'s ?
void ConsoleLogTarget::log(LogLevel level,
                           const std::string& component,
                           const std::string& message) {
  if (isatty(STDERR_FILENO)) {
    static constexpr AnsiColor::Type componentColor = AnsiColor::Cyan;
    static const auto logColor = [](LogLevel ll) -> AnsiColor::Type {
      switch (ll) {
        case LogLevel::None: return AnsiColor::Clear;
        case LogLevel::Emergency: return AnsiColor::Red;
        case LogLevel::Alert: return AnsiColor::Red;
        case LogLevel::Critical: return AnsiColor::Red;
        case LogLevel::Error: return AnsiColor::Red;
        case LogLevel::Warning: return AnsiColor::Yellow;
        case LogLevel::Notice: return AnsiColor::Green;
        case LogLevel::Info: return AnsiColor::Green;
        case LogLevel::Debug: return AnsiColor::White;
        case LogLevel::Trace: return AnsiColor::White;
      }
    };

    fprintf(stderr,
            "%s[%s] [%s] %s\n",
            createTimestamp().c_str(),
            AnsiColor::colorize(logColor(level), StringUtil::toString(level)).c_str(),
            AnsiColor::colorize(componentColor, component).c_str(),
            message.c_str());
  } else {
    fprintf(stderr,
            "%s[%s] [%s] %s\n",
            createTimestamp().c_str(),
            StringUtil::toString(level).c_str(),
            component.c_str(),
            message.c_str());
    fflush(stderr);
  }
}

std::string ConsoleLogTarget::createTimestamp() const {
  if (timestampEnabled_ == false)
    return "";

  char buf[7];
  UnixTime now = WallClock::now();
  snprintf(buf, sizeof(buf), "%06lu", now.unixMicros() % 1000000);

  return StringUtil::format("$0.$1 ",
                            now.toString("%Y-%m-%d %H:%M:%S"),
                            buf);
}

} // namespace xzero

