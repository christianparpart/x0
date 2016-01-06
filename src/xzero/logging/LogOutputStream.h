// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef _libstx_UTIL_LOGOUTPUTSTREAM_H
#define _libstx_UTIL_LOGOUTPUTSTREAM_H

#include "stx/io/outputstream.h"
#include "stx/logging/loglevel.h"
#include "stx/logging/logtarget.h"
#include "stx/stdtypes.h"

namespace stx {

class LogOutputStream : public LogTarget {
public:
  LogOutputStream(std::unique_ptr<OutputStream> target);

  void log(
      LogLevel level,
      const String& component,
      const String& message) override;

protected:
  ScopedPtr<OutputStream> target_;
};

}
#endif
