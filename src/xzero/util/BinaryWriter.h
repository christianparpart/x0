// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <functional>
#include <string>
#include <cstdint>

namespace xzero {

/**
 * encodes primitives and simple values into a binary stream
 */
class BinaryWriter {
 public:
  typedef std::function<void(const uint8_t*, size_t)> ChunkWriter;

  explicit BinaryWriter(ChunkWriter writer);

  void writeVarUInt(uint64_t value);
  void writeVarSInt64(int64_t value);
  void writeVarSInt32(int32_t value);

  void writeFixed64(uint64_t value);
  void writeFixed32(uint32_t value);

  void writeDouble(double value);
  void writeFloat(float value);

  void writeLengthDelimited(const uint8_t* data, size_t length);

  // helper
  void writeString(const std::string& str);

 private:
  ChunkWriter writer_;
};

} // namespace xzero
