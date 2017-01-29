// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/util/BinaryReader.h>

namespace xzero {

BinaryReader::BinaryReader(const uint8_t* begin, const uint8_t* end)
  : begin_(begin),
    end_(end) {
}

bool BinaryReader::parseVarUInt(uint64_t* output) {
  auto savePos = begin_;
  uint64_t result = 0;
  unsigned i = 0;

  while (begin_ != end_) {
    uint8_t byte = *begin_;
    ++begin_;

    result |= (byte & 0x7fllu) << (7 * i);

    if ((byte & 0x80u) == 0) {
      *output = result;
      return true;
    }
    ++i;
  }

  begin_ = savePos;
  return false;
}

bool BinaryReader::parseVarSInt32(int32_t* output) {
  uint64_t z;
  if (!parseVarUInt(&z))
    return false;

  *output = (z >> 1) ^ -(z & 1);
  return true;
}

bool BinaryReader::parseVarSInt64(int64_t* output) {
  uint64_t z;
  if (!parseVarUInt(&z))
    return false;

  *output = (z >> 1) ^ -(z & 1);
  return true;
}

bool BinaryReader::parseLengthDelimited(BufferRef* output) {
  auto savePos = begin_;

  uint64_t len;
  if (!parseVarUInt(&len))
    return false;

  if (begin_ + len > end_) {
    begin_ = savePos;
    return false;
  }

  *output = BufferRef((const char*) begin_, (size_t) len);
  begin_ += len;
  return true;
}

bool BinaryReader::parseFixed64(uint64_t* output) {
  if (begin_ + sizeof(uint64_t) > end_)
    return false;

  *output = *reinterpret_cast<const uint64_t*>(begin_);
  begin_ += sizeof(uint64_t);
  return true;
}

bool BinaryReader::parseFixed32(uint32_t* output) {
  if (begin_ + sizeof(uint32_t) > end_)
    return false;

  *output = *reinterpret_cast<const uint32_t*>(begin_);
  begin_ += sizeof(uint32_t);
  return true;
}

bool BinaryReader::parseDouble(double* output) {
  if (begin_ + sizeof(double) > end_)
    return false;

  *output = *reinterpret_cast<const double*>(begin_);
  begin_ += sizeof(double);
  return true;
}

bool BinaryReader::parseFloat(float* output) {
  if (begin_ + sizeof(float) > end_)
    return false;

  *output = *reinterpret_cast<const float*>(begin_);
  begin_ += sizeof(float);
  return true;
}

bool BinaryReader::parseString(std::string* output) {
  BufferRef buf;
  if (!parseLengthDelimited(&buf))
      return false;

  *output = buf.str();
  return true;
}

} // namespace xzero
