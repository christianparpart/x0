// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _libxzero_UTIL_LOGTARGET_H
#define _libxzero_UTIL_LOGTARGET_H
#include <xzero/logging/LogLevel.h>

namespace xzero {

class LogTarget {
public:
  virtual ~LogTarget() {}

  virtual void log(
      LogLevel level,
      const std::string& component,
      const std::string& message) = 0;
};

}

#endif
