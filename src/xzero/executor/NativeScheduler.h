// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/sysconfig.h>
#include <xzero/executor/PosixScheduler.h>

namespace xzero {

#if defined(__linux__)
// TODO using NativeScheduler = LinuxScheduler;
using NativeScheduler = PosixScheduler;
#else
using NativeScheduler = PosixScheduler;
#endif

} // namespace xzero

