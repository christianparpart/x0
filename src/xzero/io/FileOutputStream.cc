// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/io/FileOutputStream.h>
#include <xzero/RuntimeError.h>
#include <xzero/Buffer.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace xzero {

FileOutputStream::FileOutputStream(const std::string& path)
    : handle_(::open(path.c_str(), O_WRONLY | O_CREAT, 0666)),
      closeOnDestroy_(true) {
  if (handle_ < 0) {
    RAISE_ERRNO(errno);
  }
}

FileOutputStream::~FileOutputStream() {
  if (closeOnDestroy_) {
    ::close(handle_);
  }
}

int FileOutputStream::write(const char* buf, size_t size) {
  ssize_t n = ::write(handle_, buf, size);
  if (n < 0) {
    RAISE_ERRNO(errno);
  }
  return n;
}

} // namespace xzero
