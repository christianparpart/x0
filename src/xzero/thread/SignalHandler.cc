// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
