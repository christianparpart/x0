// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <cstdint>
#include <string>

namespace xzero {

class StringOutputStream;
class BufferOutputStream;
class FileOutputStream;

class OutputStreamVisitor {
 public:
  virtual ~OutputStreamVisitor() {}

  virtual void visit(StringOutputStream* stream) = 0;
  virtual void visit(BufferOutputStream* stream) = 0;
  virtual void visit(FileOutputStream* stream) = 0;
};

class OutputStream {
 public:
  virtual ~OutputStream() {}

  virtual int write(const char* buf, size_t size) = 0;

  int write(const std::string& data);
  int printf(const char* fmt, ...);
};

} // namespace xzero
