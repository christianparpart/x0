// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/io/FileDescriptor.h>
#include <xzero/io/FileUtil.h>
#include <unistd.h>

namespace xzero {

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
  if (fd_ >= 0) {
    FileUtil::close(fd_);
    fd_ = -1;
  }
}

}  // namespace xzero
