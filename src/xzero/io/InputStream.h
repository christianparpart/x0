// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>

namespace xzero {

class Buffer;
class OutputStream;

class StringInputStream;
class BufferInputStream;
class FileInputStream;

class InputStreamVisitor {
 public:
  virtual ~InputStreamVisitor() {}

  virtual void visit(StringInputStream* stream) = 0;
  virtual void visit(BufferInputStream* stream) = 0;
  virtual void visit(FileInputStream* stream) = 0;
};

class InputStream {
 public:
  virtual ~InputStream() {}

  virtual size_t read(Buffer* target, size_t n) = 0;
  virtual size_t transferTo(OutputStream* target) = 0;
};

} // namespace xzero
