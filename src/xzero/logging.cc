// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/sysconfig.h>
#include <xzero/defines.h>
#include <xzero/logging.h>
#include <xzero/inspect.h>
#include <xzero/StackTrace.h>
#include <xzero/AnsiColor.h>
#include <xzero/Application.h>
#include <xzero/StringUtil.h>
#include <xzero/UnixTime.h>
#include <xzero/WallClock.h>
#include <xzero/logging.h>
#include <xzero/io/FileHandle.h>
#include <xzero/io/FileUtil.h>
#include <stdexcept>
#include <iostream>
#include <stdlib.h>

#if defined(XZERO_OS_UNIX)
#include <unistd.h>
#endif

#if defined(HAVE_SYSLOG_H)
#include <syslog.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#include <io.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

namespace xzero {

// {{{ LogLevel
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
      logFatal("Illegal LogLevel enum value.");
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

  logFatal("Illegal LogLevel enum value.");
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

void Logger::fatal(const std::string& message) {
  const size_t numTargets = max_listener_index_.load();
  if (numTargets == 0)
    addTarget(ConsoleLogTarget::get());

  log(LogLevel::Fatal, message);

  std::vector<std::string> st = StackTrace().symbols();
  for (size_t i = 0, e = st.size(); i != e; ++i) {
    log(LogLevel::Fatal, fmt::format("[{}] {}", i, st[i]));
  }

  abort();
}

void Logger::error(const std::string& message) {
  log(LogLevel::Error, message);
}

void Logger::warning(const std::string& message) {
  log(LogLevel::Warning, message);
}

void Logger::notice(const std::string& message) {
  log(LogLevel::Notice, message);
}

void Logger::info(const std::string& message) {
  log(LogLevel::Info, message);
}

void Logger::debug(const std::string& message) {
  log(LogLevel::Debug, message);
}

void Logger::trace(const std::string& message) {
  log(LogLevel::Trace, message);
}

void Logger::log(LogLevel log_level, const std::string& message) {
  if (log_level >= min_level_) {
    const size_t max_idx = max_listener_index_.load();
    for (size_t i = 0; i < max_idx; ++i) {
      auto listener = listeners_[i].load();

      if (listener != nullptr) {
        listener->log(log_level, message);
      }
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
// }}}
// {{{ FileLogTarget
FileLogTarget::FileLogTarget(FileHandle&& fd)
    : fd_(std::move(fd)),
      timestampEnabled_(true) {
}

// TODO is a mutex required for concurrent printf()'s ?
void FileLogTarget::log(LogLevel level,
                        const std::string& message) {
  std::string logline = fmt::format(
      "{}[{}] {}\n",
      createTimestamp(),
      level,
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
    : timestampEnabled_(true),
      colored_{isatty(STDERR_FILENO) != 0} {
}

ConsoleLogTarget* ConsoleLogTarget::get() {
  static ConsoleLogTarget singleton;
  return &singleton;
}

// TODO is a mutex required for concurrent printf()'s ?
void ConsoleLogTarget::log(LogLevel level,
                           const std::string& message) {
  if (colored_) {
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
        default: logFatal("Invalid LogLevel.");
      }
    };

    fprintf(stderr,
            "%s[%s] %s\n",
            createTimestamp().c_str(),
            AnsiColor::colorize(logColor(level), fmt::format("{}", level)).c_str(),
            message.c_str());
  } else {
    fprintf(stderr,
            "%s[%s] %s\n",
            createTimestamp().c_str(),
            fmt::format("{}", level).c_str(),
            message.c_str());
    fflush(stderr);
  }
}

std::string ConsoleLogTarget::createTimestamp() const {
  if (timestampEnabled_ == false)
    return "";

  UnixTime now = WallClock::now();

  return fmt::format("{}.{} ",
                     now.toString("%Y-%m-%d %H:%M:%S"),
                     now.unixMicros() % 1000000);
}
// }}}
// {{{ SyslogTarget
// TODO(Win): we could instead log to the Windows System Event Log (equivalent to Unix's syslog)
SyslogTarget::SyslogTarget(const std::string& ident) {
#if defined(HAVE_SYSLOG_H)
  openlog(ident.c_str(), LOG_PID, LOG_DAEMON);
#endif
}

SyslogTarget::~SyslogTarget() {
#if defined(HAVE_SYSLOG_H)
  closelog();
#endif
}

#if defined(HAVE_SYSLOG_H)
static inline int makeSyslogPriority(LogLevel level) {
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
    default:
      logFatal("Invalid LogLevel.");
  }
}
#endif

void SyslogTarget::log(LogLevel level, const std::string& message) {
#if defined(HAVE_SYSLOG_H)
  syslog(makeSyslogPriority(level), "%s", message.c_str());
#endif
}

SyslogTarget* SyslogTarget::get() {
  static SyslogTarget st(Application::appName());
  return &st;
}
// }}}

} // namespace xzero
