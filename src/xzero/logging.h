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
#include <xzero/logging/LogAggregator.h>
#include <xzero/logging/LogLevel.h>
#include <xzero/logging/LogSource.h>
#include <string>

namespace xzero {

template<typename... Args>
XZERO_BASE_API void logError(const char* component, const char* fmt, Args... args) {
  LogAggregator::get().getSource(component)->error(fmt, args...);
}

template<typename... Args>
XZERO_BASE_API void logWarning(const char* component, const char* fmt, Args... args) {
  LogAggregator::get().getSource(component)->warn(fmt, args...);
}

template<typename... Args>
XZERO_BASE_API void logNotice(const char* component, const char* fmt, Args... args) {
  LogAggregator::get().getSource(component)->notice(fmt, args...);
}

template<typename... Args>
XZERO_BASE_API void logInfo(const char* component, const char* fmt, Args... args) {
  LogAggregator::get().getSource(component)->info(fmt, args...);
}

template<typename... Args>
XZERO_BASE_API void logDebug(const char* component, const char* fmt, Args... args) {
  LogAggregator::get().getSource(component)->debug(fmt, args...);
}

template<typename... Args>
XZERO_BASE_API void logTrace(const char* component, const char* fmt, Args... args) {
  LogAggregator::get().getSource(component)->trace(fmt, args...);
}

XZERO_BASE_API void logError(const char* component, const std::exception& e);

}  // namespace xzero
