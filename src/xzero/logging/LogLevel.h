/**
 * This file is part of the "libxzero" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * libxzero is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef _libxzero_UTIL_LOGLEVEL_H
#define _libxzero_UTIL_LOGLEVEL_H

#include <xzero/stdtypes.h>

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

std::string to_string(LogLevel value);
LogLevel to_loglevel(const std::string& value);

} // namespace xzero

#endif
