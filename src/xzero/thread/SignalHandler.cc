// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/thread/SignalHandler.h>
#include <xzero/sysconfig.h>
#include <signal.h>

// XXX on OSX, I seem to require this header to get signal() and SIG_* defs
// since as of today (lol)
#include <stdlib.h>

namespace xzero {
namespace thread {

void SignalHandler::ignore(int signum) {
  ::signal(signum, SIG_IGN);
}

void SignalHandler::ignoreSIGHUP() {
  ignore(SIGHUP);
}

void SignalHandler::ignoreSIGPIPE() {
  ignore(SIGPIPE);
}

}  // namespace thread
}  // namespace xzero
