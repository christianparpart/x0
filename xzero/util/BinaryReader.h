// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/Buffer.h>
#include <xzero/Option.h>

#include <string>
#include <cstdint>

namespace xzero {

/**
 * BinaryReader
 */
class BinaryReader {
 public:
  BinaryReader();
  BinaryReader(const uint8_t* begin, const uint8_t* end);
  BinaryReader(const uint8_t* data, size_t length);
  BinaryReader(const BufferRef& data);
  BinaryReader(const std::vector<uint8_t>& data);

  void reset(const BufferRef& data);

  Option<uint64_t> tryParseVarUInt();
  uint64_t parseVarUInt();
  int64_t parseVarSInt64();
  int32_t parseVarSInt32();
  std::vector<uint8_t> parseLengthDelimited();
  uint64_t parseFixed64();
  uint32_t parseFixed32();
  double parseDouble();
  float parseFloat();

  // helper
  std::string parseString();

  bool eof() const noexcept;
  size_t pending() const noexcept;

 private:
  const uint8_t* begin_;
  const uint8_t* end_;
};

// inlines
inline BinaryReader::BinaryReader() :
  BinaryReader(nullptr, nullptr) {
}

inline BinaryReader::BinaryReader(const uint8_t* data, size_t length) :
  BinaryReader(data, data + length) {
}

inline BinaryReader::BinaryReader(const BufferRef& data)
  : BinaryReader((const uint8_t*) data.begin(),
                 (const uint8_t*) data.end()) {
}

inline BinaryReader::BinaryReader(const std::vector<uint8_t>& data)
  : BinaryReader((const uint8_t*) &data[0],
                 (const uint8_t*) &data[0] + data.size()) {
}

inline void BinaryReader::reset(const BufferRef& data) {
  begin_ = (const uint8_t*) data.begin();
  end_ = (const uint8_t*) data.end();
}

inline bool BinaryReader::eof() const noexcept {
  return begin_ == end_;
}

inline size_t BinaryReader::pending() const noexcept {
  return end_ - begin_;
}

} // namespace xzero
