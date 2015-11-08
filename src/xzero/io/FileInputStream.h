// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/io/InputStream.h>
#include <string>

namespace xzero {

class Buffer;

class FileInputStream : public InputStream {
 public:
  explicit FileInputStream(const std::string& path);
  FileInputStream(int fd, bool closeOnDestroy);
  ~FileInputStream();

  void rewind();

  size_t read(Buffer* target, size_t n) override;
  size_t transferTo(OutputStream* target) override;

 private:
  int fd_;
  bool closeOnDestroy_;
};

} // namespace xzero
