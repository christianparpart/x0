// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libcortex is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once
#include <xzero/logging/LogTarget.h>
#include <string>

namespace xzero {

class ConsoleLogTarget : public LogTarget {
public:
  ConsoleLogTarget();

  void log(LogLevel level,
           const String& component,
           const String& message) override;

  void setTimestampEnabled(bool value) { timestampEnabled_ = value; }
  bool isTimestampEnabled() const noexcept { return timestampEnabled_; }

  static ConsoleLogTarget* get();

private:
  std::string createTimestamp() const;

private:
  bool timestampEnabled_;
};

} // namespace xzero
