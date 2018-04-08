// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/SystemPipe.h>
#include <xzero/defines.h>
#include <xzero/sysconfig.h>
#include <xzero/RuntimeError.h>

#if defined(XZERO_OS_WIN32)
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
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

void SystemPipe::setNonBlocking(bool enable) {
#if defined(XZERO_OS_WIN32)
  logFatal("Not Implemented yet.");
#else
  if (enable) {
    fcntl(fds_[0], F_SETFL, O_NONBLOCK);
    fcntl(fds_[1], F_SETFL, O_NONBLOCK);
  } else {
    for (int i = 0; i < 2; ++i) {
      int flags = fcntl(fds_[i], F_GETFL);
      if (flags != -1) {
        fcntl(fds_[i], F_SETFL, flags & ~O_NONBLOCK);
      }
    }
  }
#endif
}

void SystemPipe::closeReader() {
#if defined(XZERO_OS_WIN32)
  CloseHandle(reader_);
  writer_ = nullptr;
#else
  if (fds_[0] != -1) {
    ::close(fds_[0]);
    fds_[0] = -1;
  }
#endif
}

void SystemPipe::closeWriter() {
#if defined(XZERO_OS_WIN32)
  CloseHandle(writer_);
  writer_ = nullptr;
#else
  if (fds_[1] != -1) {
    ::close(fds_[1]);
    fds_[1] = -1;
  }
#endif
}

int SystemPipe::write(const void* buf, size_t count) {
#if defined(XZERO_OS_WIN32)
  DWORD nwritten = 0;
  WriteFile(writer_, buf, count, &nwritten, nullptr);
  return nwritten;
#else
  return ::write(writerFd(), buf, count);
#endif
}

int SystemPipe::write(const std::string& msg) {
  return write(msg.data(), msg.size());
}

void SystemPipe::consume() {
#if defined(XZERO_OS_WIN32)
  logFatal("Not Implemented yet.");
#else
  char buf[4096];
  int n;
  do n = ::read(readerFd(), buf, sizeof(buf));
  while (n > 0);
#endif
}

} // namespace xzero
