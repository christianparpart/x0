// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/Parser.h>
#include <xzero/http/http2/FrameType.h>
#include <xzero/http/http2/FrameListener.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/StringUtil.h>
#include <xzero/BufferUtil.h>
#include <xzero/Buffer.h>

#define HTTP2_STRICT 1

namespace xzero {
namespace http {
namespace http2 {

// {{{ read16 / read24 / read32 helper
template<typename T>
inline uint16_t read16(const T* buf) {
  uint16_t value = 0;

  value |= (static_cast<uint64_t>(buf[0]) & 0xFF) << 8;
  value |= (static_cast<uint64_t>(buf[1]) & 0xFF);

  return value;
}

template<typename T>
inline uint32_t read24(const T* buf) {
  uint32_t value = 0;

  value |= (static_cast<uint64_t>(buf[0]) & 0xFF) << 16;
  value |= (static_cast<uint64_t>(buf[1]) & 0xFF) << 8;
  value |= (static_cast<uint64_t>(buf[2]) & 0xFF);

  return value;
}

template<typename T>
inline uint32_t read32(const T* buf) {
  uint32_t value = 0;

  value |= (static_cast<uint64_t>(buf[0]) & 0xFF) << 24;
  value |= (static_cast<uint64_t>(buf[1]) & 0xFF) << 16;
  value |= (static_cast<uint64_t>(buf[2]) & 0xFF) << 8;
  value |= (static_cast<uint64_t>(buf[3]) & 0xFF);

  return value;
}
// }}}

constexpr uint8_t END_STREAM = 0x01;
constexpr uint8_t ACK = 0x01;
constexpr uint8_t END_HEADERS = 0x04;
constexpr uint8_t PADDED = 0x08;
constexpr uint8_t PRIORITY = 0x20;

Parser::Parser(FrameListener* listener)
    : Parser(listener, 1 << 14, 4096) {
}

Parser::Parser(FrameListener* listener,
               size_t maxFrameSize,
               size_t maxHeaderTableSize)
    : state_(State::ConnectionPreface),
      listener_(listener),
      maxFrameSize_(maxFrameSize),
      maxHeaderTableSize_(maxHeaderTableSize),
      headerContext_(maxHeaderTableSize),
      pendingHeaders_(),
      newestStreamID_(0),
      lastFrameType_(),
      lastStreamID_(0),
      dependsOnSID_(0),
      isStreamClosed_(false),
      isExclusiveDependency_(false) {
}

void Parser::setState(State newState) {
  state_ = newState;
}

size_t Parser::parseFragment(const BufferRef& chunk) {
  constexpr BufferRef ConnectionPreface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
  size_t offset = 0;

  if (state_ == State::ConnectionPreface) {
    printf("http2.Parser: state in preface\n");
    if (BufferUtil::beginsWith(chunk, ConnectionPreface)) {
      printf("http2.Parser: preface found, skipping %zu bytes\n", ConnectionPreface.size());
      setState(State::Framing);
      offset += ConnectionPreface.size();
    } else {
      listener_->onConnectionError(
          ErrorCode::ProtocolError,
          "No connection preface found.");
      return 0;
    }
  }

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

  printf("Parser.parseFrame: %s (0x%02x), flags=%02x, sid=%d, len=%zu\n",
         StringUtil::toString(type).c_str(), (int)type, flags, sid, payload.size());

  if (payload.size() > maxFrameSize_) {
    if (sid != 0)
      listener_->onStreamError(
          sid,
          ErrorCode::FrameSizeError,
          "Received frame exceeds negotiated maximum frame size.");
    else
      listener_->onConnectionError(
          ErrorCode::FrameSizeError,
          "Received frame exceeds negotiated maximum frame size.");
    return;
  }

  switch (type) {
    case FrameType::Data:
      parseData(flags, sid, payload);
      break;
    case FrameType::Headers:
      parseHeaders(flags, sid, payload);
      break;
    case FrameType::Priority:
      parsePriority(sid, payload);
      break;
    case FrameType::ResetStream:
      parseResetStream(sid, payload);
      break;
    case FrameType::Settings:
      parseSettings(flags, sid, payload);
      break;
    case FrameType::PushPromise:
      parsePushPromise(flags, sid, payload);
      break;
    case FrameType::Ping:
      parsePing(flags, sid, payload);
      break;
    case FrameType::GoAway:
      parseGoAway(flags, sid, payload);
      break;
    case FrameType::WindowUpdate:
      parseWindowUpdate(flags, sid, payload);
      break;
    case FrameType::Continuation:
      parseContinuation(flags, sid, payload);
      break;
    default:
      // silently skip this frame, and do not remember its SID nor frame type.
      return;
  }

  lastFrameType_ = type;
  lastStreamID_ = sid;
}

void Parser::parseData(uint8_t flags, StreamID sid, const BufferRef& payload) {
  /* +---------------+
   * |Pad Length? (8)|
   * +---------------+-----------------------------------------------+
   * |                            Data (*)                         ...
   * +---------------------------------------------------------------+
   * |                           Padding (*)                       ...
   * +---------------------------------------------------------------+
   */

  size_t contentLength = payload.size();
  size_t paddingLength = 0;

  if (flags & PADDED) {
    paddingLength = payload[0];
  }

  if (sid == 0) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "Invalid stream id 0 on DATA frame.");
    return;
  }

  if (paddingLength) {
    if (paddingLength >= contentLength) {
      listener_->onConnectionError(
          ErrorCode::ProtocolError,
          "DATA's padding length is larger than the frame.");
      return;
    }

    if (!verifyPadding(payload.ref(contentLength))) {
      return;
    }
  }

  const bool last = flags & END_STREAM;

  listener_->onData(sid, payload.ref(0, contentLength), last);
}

void Parser::parseHeaders(uint8_t flags,
                          StreamID sid,
                          const BufferRef& payload) {
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

  const uint8_t* pos = (uint8_t*) payload.data();
  size_t headerBlockLength = payload.size();

  size_t padlen = 0;
  if (flags & PADDED) {
    padlen = *pos;
    pos++;
  }

  if (flags & PRIORITY) {
    headerBlockLength -= 6;

    isExclusiveDependency_ = *pos & (1 << 7);
    dependsOnSID_ = read32(pos) & ~(1 << 31);
    pos += 4;

    streamWeight_ = std::min(1 + *pos, 256);
    pos++;
  } else {
    isExclusiveDependency_ = false;
  }

  // apply padlen
  if (padlen > headerBlockLength) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "HEADERS padding-length too large.");
    return;
  }

  if (sid <= newestStreamID_) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "Unexpected stream id for HEADERS frame received.");
    return;
  }

  newestStreamID_ = sid;

  headerBlockLength -= padlen;
  isStreamClosed_ = flags & END_STREAM;
  promisedStreamID_ = 0; // for indication in CONTINUATION that we're a HEADERS.

  pendingHeaders_.push_back(pos, headerBlockLength);

  if (flags & END_HEADERS) {
    onRequestBegin();
  }
}

void Parser::parsePriority(StreamID sid, const BufferRef& payload) {
  /* +-+-------------------------------------------------------------+
   * |E|                  Stream Dependency (31)                     |
   * +-+-------------+-----------------------------------------------+
   * |   Weight (8)  |
   * +-+-------------+
   */

  if (payload.size() != 5) {
    listener_->onStreamError(
        sid,
        ErrorCode::FrameSizeError,
        "PRIORITY frame size must be 5 bytes");
    return;
  }

  if (sid == 0) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "Invalid stream id 0 on PRIORITY frame.");
    return;
  }

  bool isExclusiveDependency = payload[0] & (1 << 7);
  StreamID streamDependency = read32(payload.data()) & ~(1 << 31);
  unsigned weight = payload[5] + 1;

  listener_->onPriority(sid, isExclusiveDependency, streamDependency, weight);
}

void Parser::parseResetStream(StreamID sid, const BufferRef& payload) {
  /* +---------------------------------------------------------------+
   * |                        Error Code (32)                        |
   * +---------------------------------------------------------------+
   */

  if (payload.size() != 4) {
    listener_->onConnectionError(
        ErrorCode::FrameSizeError,
        "RST_STREAM frame size must be 4 bytes");
    return;
  }

  if (sid == 0) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "Invalid stream id 0 on RST_STREAM frame.");
    return;
  }

  if (sid > newestStreamID_) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "RST_STREAM received for an idle stream.");
    return;
  }

  ErrorCode errorCode = static_cast<ErrorCode>(read32(payload.data()));

  listener_->onResetStream(sid, errorCode);
}

void Parser::parseSettings(uint8_t flags,
                           StreamID sid,
                           const BufferRef& payload) {
  /* +-------------------------------+
   * |       Identifier (16)         |
   * +-------------------------------+-------------------------------+
   * |                        Value (32)                             |
   * +---------------------------------------------------------------+
   */

  if (sid != 0) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "SETTINGS frame received an invalid stream identifier.");
    return;
  }

  if (flags & ACK) {
    if (!payload.empty()) {
      listener_->onConnectionError(
          ErrorCode::FrameSizeError,
          "SETTINGS frame with ACK flags set must not contain any payload.");
    } else {
      listener_->onSettingsAck();
    }
    return;
  }

  std::string debugData;
  std::vector<std::pair<SettingParameter, unsigned long>> settings;

  ErrorCode ec = decodeSettings(payload, &settings, &debugData);

  if (ec != ErrorCode::NoError) {
    listener_->onConnectionError(ec, debugData);
    return;
  }

  // directly apply any parser related settings
  for (const auto& setting: settings) {
    switch (setting.first) {
      case SettingParameter::HeaderTableSize:
        maxHeaderTableSize_ = setting.second;
        headerContext_.setMaxSize(setting.second);
        break;
      default:
        // ignore any unknown
        break;
    }
  }

  listener_->onSettings(settings);
}

ErrorCode Parser::decodeSettings(
    const BufferRef& payload,
    std::vector<std::pair<SettingParameter, unsigned long>>* settings,
    std::string* debugData) {
  if (payload.size() % 6 != 0) {
    *debugData = "SETTINGS frame contains wrong frame size";
    return ErrorCode::FrameSizeError;
  }

  const auto end = payload.end();
  auto pos = payload.begin();

  while (pos != end) {
    SettingParameter id = static_cast<SettingParameter>(read16(pos));
    pos += 2;

    uint32_t value = read32(pos);
    pos += 4;

    switch (id) {
      case SettingParameter::InitialWindowSize:
        if (value < (1llu << 31) - 1) {
          settings->emplace_back(id, value);
        } else {
          *debugData = "Given SETTINGS_INITIAL_WINDOW_size exceeds valid range.";
          return ErrorCode::FlowControlError;
        }
        break;
      case SettingParameter::MaxFrameSize:
        settings->emplace_back(id, value);
        // if (value >= (1 << 14) - 1 && value <= (1 << 24) - 1) {
        //   settings->emplace_back(id, value);
        // } else {
        //   *debugData = "SETTINGS frame received invalid MAX_FRAME_SIZE.";
        //   return ErrorCode::ProtocolError;
        // }
        break;
      case SettingParameter::MaxHeaderListSize:
      case SettingParameter::HeaderTableSize:
      case SettingParameter::EnablePush:
      case SettingParameter::MaxConcurrentStreams:
        settings->emplace_back(id, value);
        break;
      default:
        // ignore any unknown
        break;
    }
  }
  return ErrorCode::NoError;
}

void Parser::parsePushPromise(uint8_t flags, StreamID sid, const BufferRef& payload) {
  /* +---------------+
   * |Pad Length? (8)|
   * +-+-------------+-----------------------------------------------+
   * |R|                  Promised Stream ID (31)                    |
   * +-+-----------------------------+-------------------------------+
   * |                   Header Block Fragment (*)                 ...
   * +---------------------------------------------------------------+
   * |                           Padding (*)                       ...
   * +---------------------------------------------------------------+
   */

  auto pos = payload.begin();
  size_t contentLength = payload.size();
  size_t paddingLength = 0;

  if (flags & PADDED) {
    paddingLength = *pos;
    ++pos;
  }

  promisedStreamID_ = read32(pos) & ~(1 << 31);
  pos += 4;

  if (paddingLength > contentLength) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "PUSH_PROMISE padding-length too large.");
    return;
  }

  contentLength -= paddingLength;

  pendingHeaders_.push_back(pos, contentLength);

  if (flags & END_HEADERS) {
    onRequestBegin();
  }
}

void Parser::parsePing(uint8_t flags, StreamID sid, const BufferRef& payload) {
  /* +---------------------------------------------------------------+
   * |                      Opaque Data (64)                         |
   * +---------------------------------------------------------------+
   */

  if (payload.size() != 8) {
    listener_->onConnectionError(
        ErrorCode::FrameSizeError,
        "PING frame size must be 8 bytes.");
    return;
  }

  if (flags & ACK) {
    listener_->onPingAck(payload);
  } else {
    listener_->onPing(payload);
  }
}

void Parser::parseGoAway(uint8_t flags, StreamID sid, const BufferRef& payload) {
  /* +-+-------------------------------------------------------------+
   * |R|                  Last-Stream-ID (31)                        |
   * +-+-------------------------------------------------------------+
   * |                      Error Code (32)                          |
   * +---------------------------------------------------------------+
   * |                  Additional Debug Data (*)                    |
   * +---------------------------------------------------------------+
   */

  if (sid != 0) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "Unepxected stream id on GO_AWAY frame.");
    return;
  }

  if (payload.size() < 8) {
    listener_->onConnectionError(
        ErrorCode::FrameSizeError,
        "GO_AWAY frame size too small.");
    return;
  }

  auto pos = payload.begin();

  StreamID lastStreamID = read32(pos) & ~(1 << 31);
  pos += 4;

  ErrorCode errorCode = static_cast<ErrorCode>(read32(pos));
  pos += 4;

  BufferRef debugData(pos, payload.end() - pos);

  listener_->onGoAway(lastStreamID, errorCode, debugData);
}

void Parser::parseWindowUpdate(uint8_t flags, StreamID sid, const BufferRef& payload) {
  /* +-+-------------------------------------------------------------+
   * |R|              Window Size Increment (31)                     |
   * +-+-------------------------------------------------------------+
   */

  if (payload.size() != 4) {
    listener_->onConnectionError(
        ErrorCode::FrameSizeError,
        "WINDOW_UPDATE frame size must be 4 bytes");
  }

  uint32_t windowSizeUpdate = read32(payload.begin()) & ~(1 << 31);

  if (windowSizeUpdate == 0) {
    listener_->onStreamError(
        sid,
        ErrorCode::ProtocolError,
        "WINDOW_UPDATE received an invalid increment of 0.");
    return;
  }

  listener_->onWindowUpdate(sid, windowSizeUpdate);
}

void Parser::parseContinuation(uint8_t flags,
                               StreamID sid,
                               const BufferRef& payload) {
  if (lastStreamID_ != sid) {
    listener_->onConnectionError(
        ErrorCode::ProtocolError,
        "Interleaved CONTINUATION frame received.");
    return;
  }

  switch (lastFrameType_) {
    case FrameType::Headers:
    case FrameType::Continuation:
    case FrameType::PushPromise:
      break;
    default:
      listener_->onConnectionError(
          ErrorCode::ProtocolError,
          "A CONTINUATION frame must be following a HEADERS, PUSH_PROMISE or "
          "CONTINUATION frame.");
      return;
  }

  if (payload.empty()) {
    listener_->onConnectionError(
        ErrorCode::FrameSizeError,
        "Payload empty in CONTINUATION frame.");
    return;
  }

  if (flags & END_HEADERS) {
    onRequestBegin();
  }
}

bool Parser::verifyPadding(const BufferRef& padding) {
  for (auto ch: padding) {
    if (ch != 0x00) {
      listener_->onConnectionError(
          ErrorCode::ProtocolError,
          "DATA's padding contains non-zero bytes.");
      return false;
    }
  }
  return true;
}

static inline bool isAnyUpper(const std::string& text) {
  for (char ch: text)
    if (ch == std::toupper(ch))
      return true;

  return false;
}

void Parser::onRequestBegin() {
  HttpRequestInfo info;
  unsigned protoErrors = 0;

  auto addHeader = [&](const std::string& name,
                       const std::string& value,
                       bool sensitive) {
    printf("-- header %s: %s\n", name.c_str(), value.c_str());
    if (isAnyUpper(name))
      protoErrors++;

    info.headers().push_back(name, value, sensitive);
  };

  hpack::Parser parser(&headerContext_, maxHeaderTableSize_, addHeader);
  parser.parse(pendingHeaders_);

  if (protoErrors != 0) {
    listener_->onStreamError(
        lastStreamID_,
        ErrorCode::ProtocolError,
        "Malformed header name received.");
    return;
  }

  // XXX `info.authority` is implicitely given, as of right now
  info.setMethod(info.headers().get(":method"));
  info.setUri(info.headers().get(":path"));

  if (!promisedStreamID_) {
    // TODO: pass (dependsOnSID_, isExclusiveDependency_, streamWeight_)
    listener_->onRequestBegin(lastStreamID_, isStreamClosed_, std::move(info));
  } else {
    listener_->onPushPromise(lastStreamID_, promisedStreamID_,
                             std::move(info));
  }
}

} // namespace http2
} // namespace http
} // namespace xzero
