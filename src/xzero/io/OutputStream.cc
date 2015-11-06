// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/io/OutputStream.h>
#include <xzero/RuntimeError.h>

namespace xzero {

int OutputStream::write(const std::string& data) {
  return write(data.data(), data.size());
}

int OutputStream::printf(const char* fmt, ...) {
  char buf[8192];

  va_list args;
  va_start(args, fmt);
  int pos = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if (pos < 0) {
    RAISE_ERRNO(errno);
  }

  if (pos < sizeof(buf)) {
    write(buf, pos);
  } else {
    RAISE_ERRNO(errno);
  }

  return pos;
}

} // namespace xzero
