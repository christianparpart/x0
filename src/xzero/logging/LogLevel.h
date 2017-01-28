// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _libxzero_UTIL_LOGLEVEL_H
#define _libxzero_UTIL_LOGLEVEL_H

#include <string>

namespace xzero {

enum class LogLevel {
  None = 9999,
  Emergency = 9000,
  Alert = 8000,
  Critical = 7000,
  Error = 6000,
  Warning = 5000,
  Notice = 4000,
  Info = 3000,
  Debug = 2000,
  Trace = 1000,

  kEmergency = Emergency,
  kAlert = Alert,
  kCritical = Critical,
  kError = Error,
  kWarning = Warning,
  kNotice = Notice,
  kInfo = Info,
  kDebug = Debug,
  kTrace = Trace
};

LogLevel make_loglevel(const std::string& value);

} // namespace xzero

#endif
