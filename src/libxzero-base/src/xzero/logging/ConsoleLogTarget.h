#pragma once
#include <xzero/logging/LogTarget.h>

namespace xzero {

class ConsoleLogTarget : public LogTarget {
public:
  void log(LogLevel level,
           const String& component,
           const String& message) override;

  static ConsoleLogTarget* get();
};

} // namespace xzero
