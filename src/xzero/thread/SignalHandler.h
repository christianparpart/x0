// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <signal.h>            // for signum defs (such as: SIGPIPE, etc)

namespace xzero {
namespace thread {

class XZERO_BASE_API SignalHandler {
 public:
  static void ignore(int signum);

  static void ignoreSIGHUP();
  static void ignoreSIGPIPE();
};

} // namespace thread
} // namespace xzero
