// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/io/InputStream.h>

namespace xzero {

class Buffer;

class BufferInputStream : public InputStream {
 public:
  explicit BufferInputStream(Buffer* source);

  void rewind();

  size_t read(Buffer* target, size_t n) override;
  size_t transferTo(OutputStream* target) override;

 private:
  Buffer* source_;
  size_t offset_;
};

} // namespace xzero
