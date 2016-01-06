// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
