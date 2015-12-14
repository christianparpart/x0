// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#include <xzero/io/BufferOutputStream.h>
#include <xzero/Buffer.h>

namespace xzero {

int BufferOutputStream::write(const char* buf, size_t size) {
  buffer_->push_back(buf, size);
  return size;
}

} // namespace xzero
