// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/logging/LogLevel.h>
#include <xzero/RuntimeError.h>
#include <xzero/StringUtil.h>
#include <stdexcept>
#include <string>

namespace xzero {

const char* logLevelToStr(LogLevel log_level) {
  switch (log_level) {
    case LogLevel::kEmergency: return "EMERGENCY";
    case LogLevel::kAlert: return "ALERT";
    case LogLevel::kCritical: return "CRITICAL";
    case LogLevel::kError: return "ERROR";
    case LogLevel::kWarning: return "WARNING";
    case LogLevel::kNotice: return "NOTICE";
    case LogLevel::kInfo: return "INFO";
    case LogLevel::kDebug: return "DEBUG";
    case LogLevel::kTrace: return "TRACE";
    default: return "CUSTOM"; // FIXPAUL
  }
}

LogLevel strToLogLevel(const String& log_level) {
  if (log_level == "EMERGENCY") return LogLevel::kEmergency;
  if (log_level == "ALERT") return LogLevel::kAlert;
  if (log_level == "CRITICAL") return LogLevel::kCritical;
  if (log_level == "ERROR") return LogLevel::kError;
  if (log_level == "WARNING") return LogLevel::kWarning;
  if (log_level == "NOTICE") return LogLevel::kNotice;
  if (log_level == "INFO") return LogLevel::kInfo;
  if (log_level == "DEBUG") return LogLevel::kDebug;
  if (log_level == "TRACE") return LogLevel::kTrace;
  RAISE(IllegalArgumentError, "unknown log level");
}

std::string to_string(LogLevel value) {
  switch (value) {
    case LogLevel::None:
      return "none";
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

template<> std::string StringUtil::toString(LogLevel value) {
  return to_string(value);
}

LogLevel to_loglevel(const std::string& value) {
  if (value == "none")
    return LogLevel::None;

  if (value == "error")
    return LogLevel::Error;

  if (value == "warning")
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

} // namespace xzero
