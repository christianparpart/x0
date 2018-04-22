// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/RuntimeError.h>
#include <xzero/defines.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/io/FileUtil.h>
#include <xzero/sysconfig.h>

#if defined(XZERO_OS_WINDOWS)
#include <io.h>
#define dup _dup
#else
#include <unistd.h>
#endif

namespace xzero {

FileDescriptor::FileDescriptor(const FileDescriptor& fd)
    : fd_(fd.isOpen() ? dup(fd) : -1) {
}

FileDescriptor& FileDescriptor::operator=(const FileDescriptor& fd) {
  close();
  fd_ = fd.isOpen() ? dup(fd) : -1;

  return *this;
}

FileDescriptor& FileDescriptor::operator=(FileDescriptor&& fd) {
  close();
  fd_ = fd.release();

  return *this;
}

int FileDescriptor::release() {
  int fd = fd_;
  fd_ = -1;
  return fd;
}

void FileDescriptor::close() {
  if (fd_ < 0)
    return;

  for (;;) {
    int rv = ::close(fd_);
    switch (rv) {
      case 0:
        return;
      case EINTR:
        break;
      default:
        RAISE_ERRNO(errno);
    }
  }

  fd_ = -1;
}

}  // namespace xzero
