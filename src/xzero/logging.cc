// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/logging.h>
#include <xzero/inspect.h>
#include <xzero/AnsiColor.h>
#include <xzero/Application.h>
#include <xzero/RuntimeError.h>
#include <xzero/StringUtil.h>
#include <xzero/UnixTime.h>
#include <xzero/WallClock.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/io/FileUtil.h>
#include <stdexcept>
#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

namespace xzero {

// {{{ LogLevel
std::ostream& operator<<(std::ostream& os, LogLevel value) {
  return os << as_string(value);
}

std::string as_string(LogLevel value) {
  switch (value) {
    case LogLevel::None:
      return "none";
    case LogLevel::Fatal:
      return "fatal";
    case LogLevel::Error:
      return "error";
    case LogLevel::Warning:
      return "warning";
    case LogLevel::Notice:
      return "notice";
    case LogLevel::Info:
      return "info";
    case LogLevel::Debug:
      return "debug";
    case LogLevel::Trace:
      return "trace";
    default:
      RAISE(IllegalStateError, "Invalid State. Unknown LogLevel.");
  }
}

LogLevel make_loglevel(const std::string& str) {
  std::string value = StringUtil::toLower(str);

  if (value == "none")
    return LogLevel::None;

  if (value == "fatal")
    return LogLevel::Fatal;

  if (value == "error" || value == "err")
    return LogLevel::Error;

  if (value == "warning" || value == "warn")
    return LogLevel::Warning;

  if (value == "notice")
    return LogLevel::Notice;

  if (value == "info")
    return LogLevel::Info;

  if (value == "debug")
    return LogLevel::Debug;

  if (value == "trace")
    return LogLevel::Trace;

  RAISE(IllegalStateError, "Invalid State. Unknown LogLevel.");
}
// }}}
// {{{ Logger
Logger* Logger::get() {
  static Logger singleton;
  return &singleton;
}

Logger::Logger() :
    min_level_(LogLevel::Notice),
    max_listener_index_(0) {
  constexpr size_t e = sizeof(listeners_) / sizeof(*listeners_);
  for (size_t i = 0; i != e; ++i) {
    listeners_[i] = nullptr;
  }
}

void Logger::logException(LogLevel log_level,
                          const std::string& component,
                          const std::exception& exception,
                          const std::string& message) {
  if (log_level >= min_level_) {
    try {
      auto rte = dynamic_cast<const RuntimeError*>(&exception);
      if (rte != nullptr) {
        log(log_level,
            component,
            StringUtil::format(
                "$0: $1: $2\n    in $3\n    in $4:$5",
                message,
                rte->typeName(),
                rte->what(),
                rte->functionName(),
                rte->sourceFile(),
                rte->sourceLine()));
        if (log_level >= LogLevel::Debug) {
          rte->debugPrint(&std::cerr);
        }
      } else {
        log(log_level,
            component,
            StringUtil::format("$0: std::exception: <foreign exception> $1",
                               message,
                               exception.what()));
      }
    } catch (const std::exception& bcee) {
      log(
          log_level,
          component,
          StringUtil::format("$0: std::exception: <nested exception> $1",
                             message,
                             exception.what()));
    }
  }

  if (log_level == LogLevel::Fatal) {
    abort();
  }
}

void Logger::log(LogLevel log_level,
                 const std::string& component,
                 const std::string& message) {
  if (log_level >= min_level_) {
    const size_t max_idx = max_listener_index_.load();
    for (size_t i = 0; i < max_idx; ++i) {
      auto listener = listeners_[i].load();

      if (listener != nullptr) {
        listener->log(log_level, component, message);
      }
    }
  }

  if (log_level == LogLevel::Fatal) {
    abort();
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
// }}}
// {{{ FileLogTarget
FileLogTarget::FileLogTarget(FileDescriptor&& fd)
    : fd_(std::move(fd)),
      timestampEnabled_(true) {
}

// TODO is a mutex required for concurrent printf()'s ?
void FileLogTarget::log(LogLevel level,
                        const std::string& component,
                        const std::string& message) {
  std::string logline = StringUtil::format(
      "$0[$1] [$2] $3\n",
      createTimestamp(),
      level,
      component,
      message);

  FileUtil::write(fd_, logline);
}

std::string FileLogTarget::createTimestamp() const {
  if (timestampEnabled_ == false)
    return "";

  return WallClock::now().toString("%Y-%m-%d %H:%M:%S ");
}
// }}}
// {{{ ConsoleLogTarget
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
        case LogLevel::Fatal: return AnsiColor::Red;
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
            AnsiColor::colorize(logColor(level), to_string(level)).c_str(),
            AnsiColor::colorize(componentColor, component).c_str(),
            message.c_str());
  } else {
    fprintf(stderr,
            "%s[%s] [%s] %s\n",
            createTimestamp().c_str(),
            to_string(level).c_str(),
            component.c_str(),
            message.c_str());
    fflush(stderr);
  }
}

std::string ConsoleLogTarget::createTimestamp() const {
  if (timestampEnabled_ == false)
    return "";

  UnixTime now = WallClock::now();
  // char buf[7];
  // snprintf(buf, sizeof(buf), "%06lu", now.unixMicros() % 1000000);

  return StringUtil::format("$0.$1 ",
                            now.toString("%Y-%m-%d %H:%M:%S"),
                            xzero::to_string(now.unixMicros() % 1000000));
}
// }}}
// {{{ SyslogTarget
SyslogTarget::SyslogTarget(const std::string& ident) {
  openlog(ident.c_str(), LOG_PID, LOG_DAEMON);
}

SyslogTarget::~SyslogTarget() {
  closelog();
}

int makeSyslogPriority(LogLevel level) {
  switch (level) {
    case LogLevel::None:
      return 0; // TODO
    case LogLevel::Fatal:
      return LOG_CRIT; // XXX
    case LogLevel::Error:
      return LOG_ERR;
    case LogLevel::Warning:
      return LOG_WARNING;
    case LogLevel::Notice:
      return LOG_NOTICE;
    case LogLevel::Info:
      return LOG_INFO;
    case LogLevel::Debug:
    case LogLevel::Trace:
      return LOG_DEBUG;
  }
}

void SyslogTarget::log(LogLevel level,
                       const std::string& component,
                       const std::string& message) {

  syslog(makeSyslogPriority(level), "%s", message.c_str());
}

SyslogTarget* SyslogTarget::get() {
  static SyslogTarget st(Application::appName());
  return &st;
}
// }}}

} // namespace xzero
