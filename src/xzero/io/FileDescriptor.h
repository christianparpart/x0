// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>

namespace xzero {

/**
 * Represents a system file descriptor that gets automatically closed.
 */
class XZERO_BASE_API FileDescriptor {
 public:
  FileDescriptor() : fd_(-1) {}
  FileDescriptor(int fd) : fd_(fd) {}
  FileDescriptor(FileDescriptor&& fd) : fd_(fd.release()) {}
  ~FileDescriptor() { close(); }

  FileDescriptor& operator=(FileDescriptor&& fd);

  FileDescriptor(const FileDescriptor& fd) = delete;
  FileDescriptor& operator=(const FileDescriptor& fd) = delete;

  int get() const noexcept { return fd_; }
  operator int() const noexcept { return fd_; }

  bool isClosed() const noexcept { return fd_ < 0; }
  bool isOpen() const noexcept { return !isClosed(); }

  int release();
  void close();

 private:
  int fd_;
};

}  // namespace xzero
