// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
