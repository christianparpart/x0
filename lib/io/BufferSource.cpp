// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0/io/BufferSource.h>

namespace x0 {

BufferSource::~BufferSource() {}

ssize_t BufferSource::sendto(Sink& sink) {
  if (pos_ == buffer_.size()) return 0;

  ssize_t result = sink.write(buffer_.data() + pos_, buffer_.size() - pos_);

  if (result > 0) pos_ += result;

  return result;
}

const char* BufferSource::className() const { return "BufferSource"; }

}  // namespace x0
