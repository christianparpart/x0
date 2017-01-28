// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/io/OutputStream.h>

namespace xzero {

class Buffer;

class BufferOutputStream : public OutputStream {
 public:
  BufferOutputStream(Buffer* buffer) : buffer_(buffer) {}

  Buffer* buffer() const { return buffer_; }

  int write(const char* buf, size_t size) override;

 private:
  Buffer* buffer_;
};

} // namespace xzero
