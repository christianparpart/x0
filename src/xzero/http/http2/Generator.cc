// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/http2/Generator.h>
#include <xzero/http/http2/SettingParameter.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/net/EndPoint.h>
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

static constexpr size_t FrameHeaderSize = 9;

Generator::Generator(DataChain* sink)
    : Generator(sink, 0, 16384, 4096, 0xffff) {
}

Generator::Generator(DataChain* sink,
                     size_t paddingSize,
                     size_t maxFrameSize,
                     size_t headerTableSize,
                     size_t maxHeaderListSize)
    : sink_(sink),
      paddingSize_(paddingSize),
      maxFrameSize_(maxFrameSize)
      //, headerGenerator_(headerTableSize, maxHeaderListSize),
{
  assert(maxFrameSize_ > FrameHeaderSize + 1);
}

void Generator::setMaxFrameSize(size_t value) {
  // TODO: value = std::max(value, 16384);

  maxFrameSize_ = value;
}

void Generator::generateData(StreamID sid, const BufferRef& data, bool last) {
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

void Generator::generateSettings(
    const std::vector<std::pair<SettingParameter, unsigned>>& settings) {
  const size_t payloadSize = settings.size() * 6;
  generateFrameHeader(FrameType::Settings, 0, 0, payloadSize);

  for (const auto& param: settings) {
    write16(static_cast<unsigned>(param.first));
    write32(static_cast<unsigned>(param.second));
  }
}

void Generator::generateSettingsAcknowledgement() {
  constexpr unsigned ACK = 0x01;

  generateFrameHeader(FrameType::Settings, ACK, 0, 0);
}

void Generator::generateFrameHeader(FrameType frameType, unsigned frameFlags,
                                    StreamID streamID, size_t payloadSize) {
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

} // namespace http2
} // namespace http
} // namespace xzero
