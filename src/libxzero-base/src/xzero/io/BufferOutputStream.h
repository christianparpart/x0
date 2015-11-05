// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/io/OutputStream.h>

namespace xzero {

class BufferOutputStream : public OutputStream {
 public:
  BufferOutputStream(Buffer* sink) : sink_(sink) {}

  Buffer* buffer() const { return buffer_; }

  void write(const char* buf, size_t size) override;

 private:
  Buffer* sink_;
};

} // namespace xzero
