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
                       const String& component,
                       const String& message) {

  syslog(makeSyslogPriority(level), "%s", message.c_str());
}

SyslogTarget* SyslogTarget::get() {
  static SyslogTarget st(Application::appName());
  return &st;
}


} // namespace xzero
