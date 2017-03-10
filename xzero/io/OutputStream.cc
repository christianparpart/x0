// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/OutputStream.h>
#include <xzero/RuntimeError.h>
#include <stdarg.h>

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

  if (static_cast<size_t>(pos) < sizeof(buf)) {
    write(buf, pos);
  } else {
    RAISE_ERRNO(errno);
  }

  return pos;
}

} // namespace xzero
