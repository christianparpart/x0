// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/io/OutputStream.h>
#include <string>

namespace xzero {

class StringOutputStream : public OutputStream {
 public:
  StringOutputStream(std::string* buffer) : sink_(buffer) {}

  std::string* get() const { return sink_; }

  int write(const char* buf, size_t size) override;

 private:
  std::string* sink_;
};

} // namespace xzero
