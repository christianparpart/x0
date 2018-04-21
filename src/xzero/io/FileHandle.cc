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
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace xzero {

uint64_t FileHandle::size() const {
#if defined(XZERO_OS_WINDOWS)
  DWORD high = 0;
  DWORD low = GetFileSize(native(), &high);
  uint64_t result = high << 16 | low;
  return result;
#else
  struct stat st;
  if (fstat(native(), &st) < 0)
    RAISE_ERRNO(errno);
  else
    return st.st_size;
#endif
}

ssize_t FileHandle::read(void* buf, size_t count) {
#if defined(XZERO_OS_WINDOWS)
  DWORD nread = 0;
  if (ReadFile(native(), buf, count, &nread, nullptr))
    return nread;
  else
    return -1;
#else
  return ::read(native(), output->data() + beg, st.st_size);
#endif
}

} // namespace xzero
