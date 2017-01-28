// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>

namespace xzero {
namespace util {

class XZERO_BASE_API BinaryMessageWriter {
public:
  static const size_t kInitialDataSize = 4096;

  BinaryMessageWriter(size_t initial_size = kInitialDataSize);
  BinaryMessageWriter(void* buf, size_t buf_len);
  ~BinaryMessageWriter();

  void appendUInt16(uint16_t value);
  void appendUInt32(uint32_t value);
  void appendUInt64(uint64_t value);
  void appendString(const std::string& string);
  void append(void const* data, size_t size);

  void updateUInt16(size_t offset, uint16_t value);
  void updateUInt32(size_t offset, uint32_t value);
  void updateUInt64(size_t offset, uint64_t value);
  void updateString(size_t offset, const std::string& string);
  void update(size_t offset, void const* data, size_t size);

  void* data() const;
  size_t size() const;

protected:
  void* ptr_;
  size_t size_;
  size_t used_;
  bool owned_;
};

} // namespace util
} // namespace xzero
