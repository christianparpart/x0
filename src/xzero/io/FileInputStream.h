// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
