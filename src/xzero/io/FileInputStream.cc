// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/io/FileInputStream.h>
#include <xzero/io/FileUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace xzero {

FileInputStream::FileInputStream(const std::string& path)
    : FileInputStream(::open(path.c_str(), O_RDONLY), true) {
  if (fd_ < 0) {
    RAISE_ERRNO(errno);
  }
}

FileInputStream::FileInputStream(int fd, bool closeOnDestroy)
    : fd_(fd),
      closeOnDestroy_(closeOnDestroy) {
}

FileInputStream::~FileInputStream() {
  if (closeOnDestroy_) {
    FileUtil::close(fd_);
  }
}

void FileInputStream::rewind() {
  lseek(fd_, 0, SEEK_SET);
}

size_t FileInputStream::read(Buffer* target, size_t n) {
  target->reserve(target->size() + n);

  int rv = ::read(fd_, target->data() + target->size(), n);
  if (rv < 0)
    RAISE_ERRNO(errno);

  target->resize(target->size() + rv);

  return rv;
}

size_t FileInputStream::transferTo(OutputStream* target) {
  // TODO
  return 0;
}

} // namespace xzero
