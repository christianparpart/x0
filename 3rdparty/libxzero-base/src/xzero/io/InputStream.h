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

class Buffer;
class OutputStream;

class InputStream {
 public:
  virtual ~InputStream() {}

  virtual size_t read(Buffer* target, size_t n) = 0;
  virtual size_t transferTo(OutputStream* target) = 0;
};

class BufferInputStream {
 public:
  explicit BufferInputStream(Buffer* source);

  void rewind();

  size_t read(Buffer* target, size_t n) override;
  size_t transferTo(OutputStream* target) override;

 private:
  Buffer* source_;
  size_t offse_;
};

} // namespace xzero
