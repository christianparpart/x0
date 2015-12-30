#include <xzero/http/http2/Parser.h>
#include <xzero/http/http2/FrameType.h>
#include <xzero/http/http2/FrameListener.h>
#include <xzero/StringUtil.h>
#include <xzero/Buffer.h>

namespace xzero {
namespace http {
namespace http2 {

inline uint32_t read24(const char* buf) {
  uint32_t value = 0;

  value |= (static_cast<uint64_t>(buf[0]) & 0xFF) << 16;
  value |= (static_cast<uint64_t>(buf[1]) & 0xFF) << 8;
  value |= (static_cast<uint64_t>(buf[2]) & 0xFF);

  return value;
}

inline uint32_t read32(const char* buf) {
  uint32_t value = 0;

  value |= (static_cast<uint64_t>(buf[0]) & 0xFF) << 24;
  value |= (static_cast<uint64_t>(buf[1]) & 0xFF) << 16;
  value |= (static_cast<uint64_t>(buf[2]) & 0xFF) << 8;
  value |= (static_cast<uint64_t>(buf[3]) & 0xFF);

  return value;
}

Parser::Parser(FrameListener* listener)
    : listener_(listener),
      state_(ParserState::Idle) {
}

size_t Parser::parseFragment(const BufferRef& chunk) {
  size_t offset = 0;

  while (offset + 9 < chunk.size()) {
    uint32_t frameSize = read24(chunk.data() + offset);
    parseFrame(chunk.ref(offset));
    offset += frameSize + 9;
  }

  return offset;
}

void Parser::parseFrame(const BufferRef& frame) {
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

  FrameType type = static_cast<FrameType>(frame[3]);
  uint8_t flags = frame[4];
  StreamID sid = read32(frame.data() + 5) & ~(1 << 31);
  BufferRef payload = frame.ref(9);

  printf("Parser.parseFrame: %s\n", StringUtil::toString(type).c_str());

  switch (type) {
    case FrameType::Headers:
      headers(flags, sid, payload);
      break;
    default:
      // skip this frame
      break;
  }
}

void Parser::headers(uint8_t flags, StreamID sid, const BufferRef& payload) {
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

}

} // namespace http2
} // namespace http
} // namespace xzero
