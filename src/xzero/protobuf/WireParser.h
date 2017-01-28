// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/protobuf/WireType.h>
#include <xzero/Result.h>
#include <xzero/Buffer.h>

#include <string>
#include <cstdint>

namespace xzero {
namespace protobuf {

struct Key {
  WireType type;
  unsigned fieldNumber;
};

/**
 */
class WireParser {
 public:
  WireParser(const uint8_t* begin, const uint8_t* end); 
  WireParser(const uint8_t* data, size_t length);

  Result<uint64_t> parseVarUInt();
  Result<int64_t> parseSint64();
  Result<int32_t> parseSint32();
  Result<BufferRef> parseLengthDelimited();
  Result<double> parseFixed64();
  Result<float> parseFixed32();

  // helper
  Result<std::string> parseString();
  Result<Key> parseKey();

  bool eof() const noexcept;

 private:
  const uint8_t* begin_;
  const uint8_t* end_;
};

// inlines
inline WireParser::WireParser(const uint8_t* data, size_t length) :
  WireParser(data, data + length) {
}

inline bool WireParser::eof() const noexcept {
  return begin_ == end_;
}

} // namespace protobuf
} // namespace xzero
