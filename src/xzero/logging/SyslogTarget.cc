// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/logging/SyslogTarget.h>
#include <xzero/Application.h>

#include <syslog.h>

namespace xzero {

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
    case LogLevel::Emergency:
      return LOG_EMERG; // naeh...
    case LogLevel::Alert:
      return LOG_ALERT;
    case LogLevel::Critical:
      return LOG_CRIT;
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


} // namespace xzero
