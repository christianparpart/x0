// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/Result.h>
#include <xzero/Buffer.h>

#include <string>
#include <cstdint>

namespace xzero {

/**
 * BinaryReader
 */
class BinaryReader {
 public:
  BinaryReader(const uint8_t* begin, const uint8_t* end); 
  BinaryReader(const uint8_t* data, size_t length);

  bool parseVarUInt(uint64_t* output);
  bool parseVarSInt64(int64_t* output);
  bool parseVarSInt32(int32_t* output);
  bool parseLengthDelimited(BufferRef* output);
  bool parseFixed64(uint64_t* output);
  bool parseFixed32(uint32_t* output);
  bool parseDouble(double* output);
  bool parseFloat(float* output);

  // helper
  bool parseString(std::string* output);

  bool eof() const noexcept;

 private:
  const uint8_t* begin_;
  const uint8_t* end_;
};

// inlines
inline BinaryReader::BinaryReader(const uint8_t* data, size_t length) :
  BinaryReader(data, data + length) {
}

inline bool BinaryReader::eof() const noexcept {
  return begin_ == end_;
}

} // namespace xzero
