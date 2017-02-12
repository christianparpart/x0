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

void BinaryWriter::writeVarUInt(uint64_t value) {
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

void BinaryWriter::writeVarSInt64(int64_t n) {
  writeVarUInt((n << 1) ^ (n >> 63));
}

void BinaryWriter::writeVarSInt32(int32_t n) {
  writeVarUInt((n << 1) ^ (n >> 31));
}

void BinaryWriter::writeFixed64(uint64_t value) {
  writer_((const uint8_t*) &value, sizeof(value));
}

void BinaryWriter::writeFixed32(uint32_t value) {
  writer_((const uint8_t*) &value, sizeof(value));
}

void BinaryWriter::writeDouble(double value) {
  static_assert(sizeof(value) == 8, "sizeof(double) must be 8");
  writer_((const uint8_t*) &value, sizeof(value));
}

void BinaryWriter::writeFloat(float value) {
  static_assert(sizeof(value) == 4, "sizeof(float) must be 4");
  writer_((const uint8_t*) &value, sizeof(value));
}

void BinaryWriter::writeLengthDelimited(const uint8_t* data, size_t length) {
  writeVarUInt(length);
  writer_(data, length);
}

void BinaryWriter::writeString(const std::string& str) {
  writeLengthDelimited((const uint8_t*) str.data(), str.size());
}

} // namespace xzero
