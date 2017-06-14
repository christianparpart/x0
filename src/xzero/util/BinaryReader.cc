// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/util/BinaryReader.h>
#include <xzero/RuntimeError.h>

namespace xzero {

BinaryReader::BinaryReader(const uint8_t* begin, const uint8_t* end)
  : begin_(begin),
    end_(end) {
}

Option<uint64_t> BinaryReader::tryParseVarUInt() {
  auto savePos = begin_;
  uint64_t result = 0;
  unsigned i = 0;

  while (begin_ != end_) {
    uint8_t byte = *begin_;
    ++begin_;

    result |= (byte & 0x7fllu) << (7 * i);

    if ((byte & 0x80u) == 0) {
      return Some(result);
    }
    ++i;
  }

  begin_ = savePos;
  return None();
}

uint64_t BinaryReader::parseVarUInt() {
  Option<uint64_t> i = tryParseVarUInt();
  if (i.isNone())
    RAISE(IOError, "Not enough data.");

  return i.get();
}

int32_t BinaryReader::parseVarSInt32() {
  uint64_t z = parseVarUInt();
  return (z >> 1) ^ -(z & 1);
}

int64_t BinaryReader::parseVarSInt64() {
  uint64_t z = parseVarUInt();;
  return (z >> 1) ^ -(z & 1);
}

std::vector<uint8_t> BinaryReader::parseLengthDelimited() {
  auto savePos = begin_;
  uint64_t len = parseVarUInt();

  if (begin_ + len > end_) {
    begin_ = savePos;
    RAISE(IOError, "Not enough data.");
  }

  std::vector<uint8_t> result;
  result.resize(len);
  memcpy(result.data(), begin_, len);
  begin_ += len;
  return result;
}

uint64_t BinaryReader::parseFixed64() {
  if (begin_ + sizeof(uint64_t) > end_)
    RAISE(IOError, "Not enough data.");

  uint64_t result = *reinterpret_cast<const uint64_t*>(begin_);
  begin_ += sizeof(uint64_t);
  return result;
}

uint32_t BinaryReader::parseFixed32() {
  if (begin_ + sizeof(uint32_t) > end_)
    RAISE(IOError, "Not enough data.");

  uint32_t result = *reinterpret_cast<const uint32_t*>(begin_);
  begin_ += sizeof(uint32_t);
  return result;
}

double BinaryReader::parseDouble() {
  if (begin_ + sizeof(double) > end_)
    RAISE(IOError, "Not enough data.");

  double result = *reinterpret_cast<const double*>(begin_);
  begin_ += sizeof(double);
  return result;
}

float BinaryReader::parseFloat() {
  if (begin_ + sizeof(float) > end_)
    RAISE(IOError, "Not enough data.");

  float result = *reinterpret_cast<const float*>(begin_);
  begin_ += sizeof(float);
  return result;
}

std::string BinaryReader::parseString() {
  std::vector<uint8_t> vec = parseLengthDelimited();
  return std::string((const char*) vec.data(), vec.size());
}

} // namespace xzero
