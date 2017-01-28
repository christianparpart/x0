// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/io/StringOutputStream.h>
#include <xzero/Buffer.h>

namespace xzero {

int StringOutputStream::write(const char* buf, size_t size) {
  sink_->append(buf, size);
  return size;
}

} // namespace xzero
