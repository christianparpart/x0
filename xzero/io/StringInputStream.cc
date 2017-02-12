// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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

