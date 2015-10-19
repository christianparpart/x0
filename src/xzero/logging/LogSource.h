// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <xzero/sysconfig.h>
#include <string>

namespace std {
  class exception;
}

namespace xzero {

/**
 * A logging source.
 *
 * Creates a log message such as "[$ClassName] $LogMessage"
 *
 * Your class that needs logging creates a static member of LogSource
 * for generating logging messages.
 */
class XZERO_BASE_API LogSource {
 public:
  explicit LogSource(const std::string& component);
  ~LogSource();

  void trace(const char* fmt, ...);
  void debug(const char* fmt, ...);
  void info(const char* fmt, ...);
  void warn(const char* fmt, ...);
  void notice(const char* fmt, ...);
  void error(const char* fmt, ...);
  void error(const std::exception& e);

  void enable();
  bool isEnabled() const XZERO_BASE_NOEXCEPT;
  void disable();

  const std::string& componentName() const XZERO_BASE_NOEXCEPT { return componentName_; }

 private:
  std::string componentName_;
  bool enabled_;
};

}  // namespace xzero
