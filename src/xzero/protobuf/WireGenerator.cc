// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/protobuf/WireGenerator.h>

namespace xzero {
namespace protobuf {

WireGenerator::WireGenerator(ChunkWriter w)
  : writer_(w) {
}

void WireGenerator::generateVarUInt(uint64_t value) {
  uint8_t buf[10];
  size_t n = 0;

  do {
    buf[n] = value & 0x7fu;
    value >>= 7;
    if (value) {
      buf[n] |= 0x80u;
    }
    ++n;
  } while (value != 0);

  writer_(buf, n);
}

void WireGenerator::generateSint64(int64_t n) {
  generateVarUInt((n << 1) ^ (n >> 63));
}

void WireGenerator::generateSint32(int32_t n) {
  generateVarUInt((n << 1) ^ (n >> 31));
}

void WireGenerator::generateLengthDelimited(const uint8_t* data, size_t length) {
  generateVarUInt(length);
  writer_(data, length);
}

void WireGenerator::generateFixed64(double value) {
  static_assert(sizeof(value) == 8, "sizeof(double) must be 8");
  writer_((const uint8_t*) &value, sizeof(value));
}

void WireGenerator::generateFixed32(float value) {
  static_assert(sizeof(value) == 4, "sizeof(float) must be 4");
  writer_((const uint8_t*) &value, sizeof(value));
}

void WireGenerator::generateString(const std::string& str) {
  generateLengthDelimited((const uint8_t*) str.data(), str.size());
}

void WireGenerator::generateKey(WireType type, unsigned fieldNumber) {
  generateVarUInt((fieldNumber << 3) | uint8_t(type));
}

} // namespace protobuf
} // namespace xzero
