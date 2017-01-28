// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/logging/LogLevel.h>
#include <xzero/RuntimeError.h>
#include <xzero/StringUtil.h>
#include <stdexcept>
#include <string>

namespace xzero {

template<> std::string StringUtil::toString(LogLevel value) {
  switch (value) {
    case LogLevel::None:
      return "none";
    case LogLevel::Alert:
      return "alert";
    case LogLevel::Critical:
      return "critical";
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

  if (value == "alert")
    return LogLevel::Alert;

  if (value == "critical")
    return LogLevel::Critical;

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
