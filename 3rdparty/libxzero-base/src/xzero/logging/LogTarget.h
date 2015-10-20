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
#include <string>
#include <unordered_map>

namespace xzero {

/**
 * Log Target Interface.
 *
 * Logging target implementations (such as a console logger or syslog logger)
 * must implement this interface.
 */
class XZERO_BASE_API LogTarget {
 public:
  virtual ~LogTarget() {}

  virtual void trace(const std::string& msg) = 0;
  virtual void debug(const std::string& msg) = 0;
  virtual void info(const std::string& msg) = 0;
  virtual void notice(const std::string& msg) = 0;
  virtual void warn(const std::string& msg) = 0;
  virtual void error(const std::string& msg) = 0;

  static LogTarget* console(); // standard console logger
  static LogTarget* syslog();  // standard syslog logger
  static LogTarget* null();
};

}  // namespace xzero
