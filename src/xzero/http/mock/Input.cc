// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/mock/Input.h>

namespace xzero {
namespace http {
namespace mock {

Input::Input()
    : buffer_() {
}

void Input::rewind() {
}

int Input::read(Buffer* result) {
  size_t n = buffer_.size();
  result->push_back(buffer_);
  buffer_.clear();
  return n;
}

size_t Input::readLine(Buffer* result) {
  return 0; // TODO
}

void Input::onContent(const BufferRef& chunk) {
  buffer_ += chunk;
}

bool Input::empty() const noexcept {
  return buffer_.empty();
}

void Input::recycle() {
  buffer_.clear();
}

} // namespace mock
} // namespace http
} // namespace xzero
