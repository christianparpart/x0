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

class XZERO_BASE_API OutputStream {
 public:
  virtual OutputStream() {}

  virtual void write(const char* buf, size_t size) = 0;
  virtual void write(const std::string& data);
  virtual void printf(const char* fmt, ...);
};

class XZERO_BASE_API FileOutputStream : public OutputStream {
 public:
  FileOutputStream(int fd);
  ~FileOutputStream();

  int handle() const XZERO_BASE_NOEXCEPT { return fd_; }

 private:
  int fd_;
};

class XZERO_BASE_API BufferOutputStream : public OutputStream {
 public:
  BufferOutputStream(Buffer* sink);

 private:
  Buffer* sink_;
};

} // namespace xzero


















