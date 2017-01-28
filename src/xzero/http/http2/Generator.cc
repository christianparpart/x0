// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/Generator.h>
#include <xzero/http/http2/SettingParameter.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/http/HeaderField.h>
#include <xzero/io/DataChain.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>
#include <assert.h>

namespace xzero {
namespace http {
namespace http2 {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("http.http2.Generator", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

constexpr size_t FrameHeaderSize = 9;

constexpr size_t InitialHeaderTableSize = 4096;
//constexpr size_t InitialMaxConcurrentStreams = 0x7fffffff; // (infinite)
//constexpr size_t InitialWindowSize = 65535;
constexpr size_t InitialMaxFrameSize = 16384;
constexpr size_t InitialMaxHeaderListSize = 0x7fffffff; // (infinite)

Generator::Generator(DataChain* sink)
    : Generator(sink,
                InitialMaxFrameSize,
                InitialHeaderTableSize,
                InitialMaxHeaderListSize) {
}

Generator::Generator(DataChain* sink,
                     size_t maxFrameSize,
                     size_t headerTableSize,
                     size_t maxHeaderListSize)
    : sink_(sink),
      maxFrameSize_(maxFrameSize),
      headerGenerator_(headerTableSize/*TODO: solve this, maxHeaderListSize*/) {
  assert(maxFrameSize_ > FrameHeaderSize + 1);
}

void Generator::setMaxFrameSize(size_t value) {
  value = std::max(value, static_cast<size_t>(16384));

  maxFrameSize_ = value;
}

void Generator::generateClientConnectionPreface() {
  sink_->write("PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n");
}

void Generator::generateData(StreamID sid, const BufferRef& data, bool last) {
  /*
   * +---------------+
   * |Pad Length? (8)|
   * +-+-------------+-----------------------------------------------+
   * |E|                 Stream Dependency? (31)                     |
   * +-+-------------+-----------------------------------------------+
   * |  Weight? (8)  |
   * +-+-------------+-----------------------------------------------+
   * |                   Header Block Fragment (*)                 ...
   * +---------------------------------------------------------------+
   * |                           Padding (*)                       ...
   * +---------------------------------------------------------------+
   */
  assert(sid != 0);

  constexpr unsigned END_STREAM = 0x01;
  //constexpr unsigned PADDED = 0x08;

  const size_t maxPayloadSize = maxFrameSize();
  size_t offset = 0;

  for (;;) {
    const size_t remainingDataSize = data.size() - offset;
    const size_t payloadSize = std::min(remainingDataSize, maxPayloadSize);
    const unsigned flags = !last && payloadSize < remainingDataSize
                         ? 0
                         : END_STREAM;

    generateFrameHeader(FrameType::Data, flags, sid, payloadSize);
    sink_->write(data.ref(offset, payloadSize));

    offset += payloadSize;

    if (offset == data.size())
      break;
  }
}

void Generator::generateHeaders(StreamID sid, const HeaderFieldList& headers,
                                bool last) {
  StreamID dependsOnSID = 0;
  bool isExclusive = false;
  uint8_t weight = 0;
  generateHeaders(sid, headers, last, dependsOnSID, isExclusive, weight);
}

void Generator::generateHeaders(StreamID sid, const HeaderFieldList& headers,
                                StreamID dependsOnSID,
                                bool isExclusive,
                                uint8_t weight,
                                bool last) {
  /* +---------------+
   * |Pad Length? (8)|
   * +-+-------------+-----------------------------------------------+
   * |E|                 Stream Dependency? (31)                     |
   * +-+-------------+-----------------------------------------------+
   * |  Weight? (8)  |
   * +-+-------------+-----------------------------------------------+
   * |                   Header Block Fragment (*)                 ...
   * +---------------------------------------------------------------+
   * |                           Padding (*)                       ...
   * +---------------------------------------------------------------+
   */
  constexpr unsigned END_STREAM = 0x01;
  constexpr unsigned END_HEADERS = 0x04;
  //constexpr unsigned PADDED = 0x08;
  constexpr unsigned PRIORITY = 0x20;

  assert(sid != 0);

  headerGenerator_.clear();
  for (const HeaderField& field: headers)
    headerGenerator_.generateHeader(field.name(), field.value(),
                                    field.isSensitive());

  const BufferRef& payload = headerGenerator_.headerBlock();

  unsigned flags = 0;

  if (last)
    flags |= END_STREAM;

  if (dependsOnSID)
    flags |= PRIORITY;

  if (payload.size() <= maxFrameSize_) {
    flags |= END_HEADERS;

    generateFrameHeader(FrameType::Headers, flags, sid, payload.size());
    if (dependsOnSID) {
      if (isExclusive)
        write32(dependsOnSID | (1 << 31));
      else
        write32(dependsOnSID & ~(1 << 31));
      write8(weight);
    }
  } else {
    generateFrameHeader(FrameType::Headers, flags, sid, maxFrameSize_);
    if (dependsOnSID) {
      if (isExclusive)
        write32(dependsOnSID | (1 << 31));
      else
        write32(dependsOnSID & ~(1 << 31));
      write8(weight);
    }
    sink_->write(payload.ref(0, maxFrameSize_));
    generateContinuations(sid, payload.ref(maxFrameSize_));
  }
}

void Generator::generatePriority(StreamID sid, bool exclusive,
                                 StreamID dependantStreamID, unsigned weight) {
  /*
   * +-+-------------------------------------------------------------+
   * |E|                  Stream Dependency (31)                     |
   * +-+-------------+-----------------------------------------------+
   * |   Weight (8)  |
   * +-+-------------+
   */

  assert(1 <= weight && weight <= 256);
  assert(sid != 0);

  generateFrameHeader(FrameType::Priority, 0, sid, 5);
  write32(dependantStreamID | (exclusive ? (1 << 31) : 0)); // bit 31 is the Exclusive-bit
  write8(weight - 1);
}

void Generator::generateResetStream(StreamID sid, ErrorCode errorCode) {
  /*
   *  +---------------------------------------------------------------+
   *  |                        Error Code (32)                        |
   *  +---------------------------------------------------------------+
   */

  const unsigned flags = 0;
  const uint32_t payload = static_cast<uint32_t>(errorCode);

  generateFrameHeader(FrameType::ResetStream, flags, sid, sizeof(payload));
  write32(payload);
}

void Generator::generateSettings(
    const std::vector<std::pair<SettingParameter, unsigned>>& settings) {
  /* a multiple of:
   *
   * +-------------------------------+
   * |       Identifier (16)         |
   * +-------------------------------+-------------------------------+
   * |                        Value (32)                             |
   * +---------------------------------------------------------------+
   */

  const size_t payloadSize = settings.size() * 6;
  generateFrameHeader(FrameType::Settings, 0, 0, payloadSize);

  for (const auto& param: settings) {
    write16(static_cast<unsigned>(param.first));
    write32(static_cast<unsigned>(param.second));
  }
}

void Generator::generateSettingsAck() {
  constexpr unsigned ACK = 0x01;

  generateFrameHeader(FrameType::Settings, ACK, 0, 0);
}

void Generator::generatePushPromise(StreamID sid, StreamID psid,
                                    const HttpRequestInfo& info) {
  /*
   * +---------------+
   * |Pad Length? (8)|
   * +-+-------------+-----------------------------------------------+
   * |R|                  Promised Stream ID (31)                    |
   * +-+-----------------------------+-------------------------------+
   * |                   Header Block Fragment (*)                 ...
   * +---------------------------------------------------------------+
   * |                           Padding (*)                       ...
   * +---------------------------------------------------------------+
   */

  static constexpr unsigned END_HEADERS = 0x04;
  // static constexpr unsigned PADDED = 0x08;

  headerGenerator_.clear();
  headerGenerator_.generateHeader(":method", info.unparsedMethod());
  headerGenerator_.generateHeader(":path", info.path());
  headerGenerator_.generateHeader(":scheme", info.scheme());
  headerGenerator_.generateHeader(":authority", info.authority());
  for (const HeaderField& field: info.headers()) {
    if (field.name().empty() || field.name()[0] == ':')
      continue;

    headerGenerator_.generateHeader(field.name(), field.value(),
                                    field.isSensitive());
  }

  const BufferRef& payload = headerGenerator_.headerBlock();

  if (payload.size() <= maxFrameSize_) {
    generateFrameHeader(FrameType::PushPromise, END_HEADERS, sid, payload.size());
    write32(psid & ~(1 << 31)); // promised id with R-flag cleared.
    sink_->write(payload);
  } else {
    generateFrameHeader(FrameType::PushPromise, 0, sid, maxFrameSize_);
    sink_->write(payload.ref(0, maxFrameSize_));
    generateContinuations(sid, payload.ref(maxFrameSize_));
  }
}

void Generator::generateContinuations(StreamID sid, const BufferRef& payload) {
  static constexpr unsigned END_HEADERS = 0x04;

  auto pos = payload.data();
  size_t count = payload.size();

  // any middle-CONTINUATION frames
  while (count > maxFrameSize_) {
    generateFrameHeader(FrameType::Continuation, 0, sid, maxFrameSize_);
    sink_->write(pos, maxFrameSize_);

    count -= maxFrameSize_;
    pos += maxFrameSize_;
  }

  // last CONTINUATION frame
  generateFrameHeader(FrameType::Continuation, END_HEADERS, sid, count);
  sink_->write(pos, count);
}

void Generator::generatePing(uint64_t payload) {
  /*
   * +---------------------------------------------------------------+
   * |                      Opaque Data (64)                         |
   * +---------------------------------------------------------------+
   */

  generateFrameHeader(FrameType::Ping, 0, 0, 8);
  write64(payload);
}

void Generator::generatePing(const BufferRef& payload) {
  /*
   * +---------------------------------------------------------------+
   * |                      Opaque Data (64)                         |
   * +---------------------------------------------------------------+
   */

  assert(payload.size() == 8);
  generateFrameHeader(FrameType::Ping, 0, 0, 8);
  sink_->write(payload);
}

void Generator::generatePingAck(const BufferRef& payload) {
  /*
   * +---------------------------------------------------------------+
   * |                      Opaque Data (64)                         |
   * +---------------------------------------------------------------+
   */

  assert(payload.size() == 8);
  static constexpr unsigned ACK = 0x01;
  generateFrameHeader(FrameType::Ping, ACK, 0, 8);
  sink_->write(payload);
}

void Generator::generatePingAck(uint64_t payload) {
  /*
   * +---------------------------------------------------------------+
   * |                      Opaque Data (64)                         |
   * +---------------------------------------------------------------+
   */

  static constexpr unsigned ACK = 0x01;
  generateFrameHeader(FrameType::Ping, ACK, 0, 8);
  write64(payload);
}

void Generator::generateGoAway(StreamID lastStreamID,
                               ErrorCode errorCode,
                               const BufferRef& debugData) {
  /*
   * +-+-------------------------------------------------------------+
   * |R|                  Last-Stream-ID (31)                        |
   * +-+-------------------------------------------------------------+
   * |                      Error Code (32)                          |
   * +---------------------------------------------------------------+
   * |                  Additional Debug Data (*)                    |
   * +---------------------------------------------------------------+
   */

  const size_t debugDataSize = std::min(maxFrameSize() - 8, debugData.size());

  assert(lastStreamID != 0);

  generateFrameHeader(FrameType::GoAway, 0, lastStreamID, debugDataSize + 8);
  write32(lastStreamID & ~(1 << 31)); // R-bit cleared out
  write32(static_cast<uint32_t>(errorCode));
  sink_->write(debugData.data(), debugDataSize);
}

void Generator::generateWindowUpdate(StreamID sid, size_t size) {
  /*
   * +-+-------------------------------------------------------------+
   * |R|              Window Size Increment (31)                     |
   * +-+-------------------------------------------------------------+
   */

  assert(size >= 1 && size <= 0x7fffffffllu); // between 1 and 2^31

  generateFrameHeader(FrameType::WindowUpdate, 0, sid, 4);
  write32(size & ~(1 << 31)); // R-bit cleared out
}

void Generator::generateFrameHeader(FrameType frameType, unsigned frameFlags,
                                    StreamID streamID, size_t payloadSize) {
  /*
   * +-----------------------------------------------+
   * |                 Length (24)                   |
   * +---------------+---------------+---------------+
   * |   Type (8)    |   Flags (8)   |
   * +-+-------------+---------------+-------------------------------+
   * |R|                 Stream Identifier (31)                      |
   * +=+=============================================================+
   * |                   Frame Payload (0...)                      ...
   * +---------------------------------------------------------------+
   */

  TRACE("header: type:$0 flags:$1, sid:$2, payloadSize:$3",
      frameType, frameFlags, streamID, payloadSize);

  write24(payloadSize);
  write8((unsigned) frameType);
  write8(frameFlags);
  write32(streamID & ~(1 << 31)); // XXX bit 31 is cleared out (reserved)
}

void Generator::write8(unsigned value) {
  sink_->write8(value);
}

void Generator::write16(unsigned value) {
  sink_->write16(value);
}

void Generator::write24(unsigned value) {
  sink_->write24(value);
}

void Generator::write32(unsigned value) {
  sink_->write32(value);
}

void Generator::write64(uint64_t value) {
  sink_->write64(value);
}

} // namespace http2
} // namespace http
} // namespace xzero
