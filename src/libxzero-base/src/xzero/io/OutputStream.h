// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <xzero/sysconfig.h>
#include <cstdint>

namespace xzero {

class OutputStreamVisitor {
 public:
  virtual ~OutputStreamVisitor() {}

  virtual void visit(FileOutputStream* stream) = 0;
  virtual void visit(BufferOutputStream* stream) = 0;
};

class OutputStream {
 public:
  virtual ~OutputStream() {}

  virtual void write(const char* buf, size_t size) = 0;

  void write(const std::string& data);
  void printf(const char* fmt, ...);
};

} // namespace xzero
