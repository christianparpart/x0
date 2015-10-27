/**
 * This file is part of the "libxzero" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * libxzero is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef _libxzero_UTIL_LOGTARGET_H
#define _libxzero_UTIL_LOGTARGET_H
#include <xzero/logging/LogLevel.h>

namespace xzero {

class LogTarget {
public:
  virtual ~LogTarget() {}

  virtual void log(
      LogLevel level,
      const String& component,
      const String& message) = 0;
};

}

#endif
