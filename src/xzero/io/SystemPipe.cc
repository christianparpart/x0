// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/SystemPipe.h>
#include <xzero/RuntimeError.h>

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined(XZERO_OS_WIN32)
#include <Windows.h>
#endif

namespace xzero {

SystemPipe::SystemPipe() {
#if defined(XZERO_OS_WIN32)
  if (CreatePipe(&reader_, &writer_, nullptr, 4096) == FALSE) {
    // TODO: handle error with `DWORD GetLastError();`
  }
#else
  if (pipe(fds_) < 0) {
    RAISE_ERRNO(errno);
  }
#endif
}

SystemPipe::~SystemPipe() {
  closeReader();
  closeWriter();
}

int SystemPipe::write(const std::string& msg) {
#if defined(XZERO_OS_WIN32)
  DWORD nwritten = 0;
  WriteFile(writer_, msg.data(), msg.size(), &nwritten, nullptr);
  return nwritten;
#else
  return ::write(writerFd(), msg.data(), msg.size());
#endif
}

} // namespace xzero
