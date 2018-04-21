// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/RuntimeError.h>
#include <xzero/defines.h>
#include <xzero/io/FileHandle.h>
#include <xzero/sysconfig.h>

#include <cerrno>

#if defined(XZERO_OS_UNIX)
#include <unistd.h>
#include <fcntl.h>
#endif

#if defined(XZERO_OS_WINDOWS)
#include <Windows.h>
#endif

namespace xzero {

} // namespace xzero
