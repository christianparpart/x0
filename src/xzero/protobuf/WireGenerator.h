// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/protobuf/WireType.h>

#include <functional>
#include <string>
#include <cstdint>

namespace xzero {
namespace protobuf {

/**
 * WireGenerator encodes primitives into the wire.
 */
class WireGenerator {
 public:
  typedef std::function<void(const uint8_t*, size_t)> ChunkWriter;

  explicit WireGenerator(ChunkWriter);

  void generateVarUInt(uint64_t value);
  void generateSint64(int64_t value);
  void generateSint32(int32_t value);
  void generateLengthDelimited(const uint8_t* data, size_t length);
  void generateFixed64(double value);
  void generateFixed32(float value);

  // helper
  void generateString(const std::string& str);
  void generateKey(WireType type, unsigned fieldNumber);

 private:
  ChunkWriter writer_;
};

} // namespace protobuf
} // namespace xzero
