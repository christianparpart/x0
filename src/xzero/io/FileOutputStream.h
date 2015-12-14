// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/io/OutputStream.h>

namespace xzero {

class FileOutputStream : public OutputStream {
 public:
  FileOutputStream(const std::string& path, int mode);

  FileOutputStream(int handle, bool closeOnDestroy)
      : handle_(handle), closeOnDestroy_(closeOnDestroy) {}

  ~FileOutputStream();

  int handle() const noexcept { return handle_; }

  // OutputStream overrides
  int write(const char* buf, size_t size) override;

 private:
  int handle_;
  bool closeOnDestroy_;
};

} // namespace xzero
