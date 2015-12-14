// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#include <xzero/io/StringInputStream.h>
#include <xzero/Buffer.h>

namespace xzero {

StringInputStream::StringInputStream(const std::string* source)
    : source_(source),
      offset_(0) {
}

void StringInputStream::rewind() {
  offset_ = 0;
}

size_t StringInputStream::read(Buffer* target, size_t n) {
  n = std::max(n, source_->size() - offset_);
  target->push_back(source_->data() + offset_, n);
  offset_ += n;
  return n;
}

size_t StringInputStream::transferTo(OutputStream* target) {
  // TODO
  return 0;
}

} // namespace xzero

