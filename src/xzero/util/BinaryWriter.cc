// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/util/BinaryWriter.h>
#include <arpa/inet.h>

namespace xzero {

BinaryWriter::BinaryWriter(ChunkWriter w)
  : writer_(w) {
}

void BinaryWriter::generateVarUInt(uint64_t value) {
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

void BinaryWriter::generateVarSInt64(int64_t n) {
  generateVarUInt((n << 1) ^ (n >> 63));
}

void BinaryWriter::generateVarSInt32(int32_t n) {
  generateVarUInt((n << 1) ^ (n >> 31));
}

void BinaryWriter::generateFixed64(uint64_t value) {
  uint32_t n[2] = {
    htonl(value >> 32),
    htonl(value & 0xFFFFFFFFu),
  };
  writer_((const uint8_t*) &n, 8);
}

void BinaryWriter::generateFixed32(uint32_t value) {
  uint32_t n = htonl(value);
  writer_((const uint8_t*) &n, sizeof(n));
}

void BinaryWriter::generateDouble(double value) {
  static_assert(sizeof(value) == 8, "sizeof(double) must be 8");
  writer_((const uint8_t*) &value, sizeof(value));
}

void BinaryWriter::generateFloat(float value) {
  static_assert(sizeof(value) == 4, "sizeof(float) must be 4");
  writer_((const uint8_t*) &value, sizeof(value));
}

void BinaryWriter::generateLengthDelimited(const uint8_t* data, size_t length) {
  generateVarUInt(length);
  writer_(data, length);
}

void BinaryWriter::generateString(const std::string& str) {
  generateLengthDelimited((const uint8_t*) str.data(), str.size());
}

} // namespace xzero
