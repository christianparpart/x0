// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <atomic>
#include <xzero/io/FileHandle.h>
#include <fmt/format.h>

namespace xzero {

enum class LogLevel { // {{{
  None = 9999,
  Fatal = 7000,
  Error = 6000,
  Warning = 5000,
  Notice = 4000,
  Info = 3000,
  Debug = 2000,
  Trace = 1000,
};

LogLevel make_loglevel(const std::string& value);

std::string as_string(LogLevel value);
// }}}
class LogTarget { // {{{
 public:
  virtual ~LogTarget() {}

  virtual void log(LogLevel level, const std::string& message) = 0;
};
// }}}
class FileLogTarget : public LogTarget { // {{{
 public:
  explicit FileLogTarget(FileHandle&& fd);

  void log(LogLevel level, const std::string& message) override;

  void setTimestampEnabled(bool value) { timestampEnabled_ = value; }
  bool isTimestampEnabled() const noexcept { return timestampEnabled_; }

 private:
  std::string createTimestamp() const;

 private:
  FileHandle fd_;
  bool timestampEnabled_;
}; // }}}
class ConsoleLogTarget : public LogTarget { // {{{
 public:
  ConsoleLogTarget();

  void log(LogLevel level, const std::string& message) override;

  void setTimestampEnabled(bool value) { timestampEnabled_ = value; }
  bool isTimestampEnabled() const noexcept { return timestampEnabled_; }

  static ConsoleLogTarget* get();

 private:
  std::string createTimestamp() const;

 private:
  bool timestampEnabled_;
  bool colored_;
}; // }}}
class SyslogTarget : public LogTarget { // {{{
 public:
  explicit SyslogTarget(const std::string& ident);
  ~SyslogTarget();

  void log(LogLevel level, const std::string& message) override;

  static SyslogTarget* get();
};
// }}}
class Logger { // {{{
 public:
  Logger();
  static Logger* get();

  [[noreturn]] void fatal(const std::string& message);
  void error(const std::string& message);
  void warning(const std::string& message);
  void notice(const std::string& message);
  void info(const std::string& message);
  void debug(const std::string& message);
  void trace(const std::string& message);

  void addTarget(LogTarget* target);
  void setMinimumLogLevel(LogLevel min_level);
  LogLevel getMinimumLogLevel() { return min_level_.load(); }

 protected:
  void log(LogLevel log_level, const std::string& message);

 protected:
  std::atomic<LogLevel> min_level_;
  std::atomic<size_t> max_listener_index_;
  std::atomic<LogTarget*> listeners_[64];
};
// }}}
// {{{ free functions
/**
 * CRITICAL: Action should be taken as soon as possible
 */
template <typename... T>
[[noreturn]] void logFatal(const std::string& msg, T... args) {
  Logger::get()->fatal(fmt::format(msg, args...));
}

#define XZERO_ASSERT(cond, msg) \
  if (!(cond)) { \
    logFatal((__FILE__ ":") + std::to_string(__LINE__) + ": " + msg); \
  }

/**
 * ERROR: User-visible Runtime Errors
 */
template <typename... T>
inline void logError(const std::string& msg, T... args) {
  Logger::get()->error(fmt::format(msg, args...));
}

/**
 * WARNING: Something unexpected happened that should not have happened
 */
template <typename... T>
inline void logWarning(const std::string& msg, T... args) {
  Logger::get()->warning(fmt::format(msg, args...));
}

/**
 * NOTICE: Normal but significant condition.
 */
template <typename... T>
inline void logNotice(const std::string& msg, T... args) {
  Logger::get()->notice(fmt::format(msg, args...));
}

/**
 * INFO: Informational messages
 */
template <typename... T>
inline void logInfo(const std::string& msg, T... args) {
  Logger::get()->info(fmt::format(msg, args...));
}

/**
 * DEBUG: Debug messages
 */
template <typename... T>
inline void logDebug(const std::string& msg, T... args) {
  Logger::get()->debug(fmt::format(msg, args...));
}

/**
 * TRACE: Trace messages
 */
template <typename... T>
inline void logTrace(const std::string& msg, T... args) {
  Logger::get()->trace(fmt::format(msg, args...));
}
// }}}

} // namespace xzero

namespace fmt {
  template<>
  struct formatter<xzero::LogLevel> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::LogLevel& v, FormatContext &ctx) {
      using xzero::LogLevel;

      switch (v) {
        case LogLevel::None: return format_to(ctx.begin(), "none");
        case LogLevel::Fatal: return format_to(ctx.begin(), "fatal");
        case LogLevel::Error: return format_to(ctx.begin(), "error");
        case LogLevel::Warning: return format_to(ctx.begin(), "warning");
        case LogLevel::Notice: return format_to(ctx.begin(), "notice");
        case LogLevel::Info: return format_to(ctx.begin(), "info");
        case LogLevel::Debug: return format_to(ctx.begin(), "debug");
        case LogLevel::Trace: return format_to(ctx.begin(), "trace");
        default:
          return format_to(ctx.begin(), "({})", (int) v);
      }
    }
  };
}
