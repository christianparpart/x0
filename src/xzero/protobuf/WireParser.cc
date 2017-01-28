// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#include <xzero/protobuf/WireParser.h>

namespace xzero {
namespace protobuf {

WireParser::WireParser(const uint8_t* begin, const uint8_t* end)
  : begin_(begin),
    end_(end) {
}

Result<uint64_t> WireParser::parseVarUInt() {
  auto savePos = begin_;
  uint64_t result = 0;
  unsigned i = 0;

  while (begin_ != end_) {
    uint8_t byte = *begin_;
    ++begin_;

    result |= (byte & 0x7fllu) << (7 * i);

    if ((byte & 0x80u) == 0) {
      return Result<uint64_t>(result);
    }
    ++i;
  }

  begin_ = savePos;
  return Failure("Not enough data");
}

Result<int32_t> WireParser::parseSint32() {
  Result<uint64_t> i = parseVarUInt();
  if (i.isFailure())
    return Failure(i.failureMessage());

  if (*i & 1)
    return Result<int32_t>(-1 - (*i >> 1));
  else
    return Result<int32_t>(*i >> 1);
}

Result<int64_t> WireParser::parseSint64() {
  Result<uint64_t> i = parseVarUInt();
  if (i.isFailure())
    return Failure(i.failureMessage());

  if (*i & 1)
    return Result<int64_t>(-1 - (*i >> 1));
  else
    return Result<int64_t>(*i >> 1);
}

Result<BufferRef> WireParser::parseLengthDelimited() {
  auto savePos = begin_;

  Result<uint64_t> len = parseVarUInt();
  if (len.isFailure())
    return Failure(len.failureMessage());

  if (begin_ + *len > end_) {
    begin_ = savePos;
    return Failure("Not enough data");
  }

  Result<BufferRef> result = BufferRef((const char*) begin_, (size_t) *len);
  begin_ += *len;
  return result;
}

Result<double> WireParser::parseFixed64() {
  if (begin_ + sizeof(double) > end_)
    return Failure("Not enough data");

  Result<double> r = *reinterpret_cast<const double*>(begin_);
  begin_ += sizeof(double);
  return r;
}

Result<float> WireParser::parseFixed32() {
  if (begin_ + sizeof(float) > end_)
    return Failure("Not enough data");

  Result<float> r = *reinterpret_cast<const float*>(begin_);
  begin_ += sizeof(float);
  return r;
}

Result<std::string> WireParser::parseString() {
  Result<BufferRef> buf = parseLengthDelimited();
  if (buf.isFailure())
    return Failure(buf.failureMessage());

  return Success(buf->str());
}

Result<Key> WireParser::parseKey() {
  Result<uint64_t> i = parseVarUInt();
  if (i.isFailure())
    return Failure(i.failureMessage());

  return Result<Key>(Key{ .type = static_cast<WireType>(*i & 7u),
                          .fieldNumber = static_cast<unsigned>(*i >> 3) });
}

} // namespace protobuf
} // namespace xzero
