// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/http/http2/StreamID.h>
#include <xzero/http/http2/FrameType.h>
#include <xzero/http/http2/SettingParameter.h>
#include <xzero/defines.h>
#include <utility>
#include <vector>
#include <stdint.h>

namespace xzero {

class BufferRef;
class FileView;
class DataChain;

namespace http {

class HeaderFieldList;
class HttpResponseInfo;

namespace http2 {

enum class ErrorCode;

#if 0
class XZERO_PACKED Frame {
 public:
  Frame(StreamID sid, FrameType type, unsigned flags);

  size_t payloadSize() const noexcept { return length_; }
  FrameType type() const noexcept { return (FrameType) type_; }
  unsigned flags() const noexcept { return flags_; }
  StreamID streamID() const noexcept { return streamID_; }
  const uint8_t* payload() const noexcept { return payload_; }

 protected:
  unsigned length_ : 24;
  unsigned type_ : 8;
  unsigned flags_ : 8;
  unsigned reserved_ : 1;
  unsigned streamID_ : 31;
  uint8_t payload_[];
};

class XZERO_PACKED DataFrame : public Frame {
 public:
  static constexpr END_STREAM = 0x01;
  static constexpr PADDING = 0x08;

  DataFrame(StreamID sid, const BufferRef& data, unsigned flags)
      : Frame(sid, FrameType::Data, flags) {
    std::memcpy(payload_, data.data(), data.size());
  }

  bool isEndStream() const { return flags() & END_STREAM; }
  bool isPadded() const { return flags() & PADDED; }

  size_t dataSize() const {
    return isPadded() ? payloadSize() - paddingLength_ : payloadSize();
  }

  size_t paddingSize() const {
    return isPadded() ? paddingLength_ : 0;
  }

  BufferRef data() const {
    return isPadded()
        ? BufferRef(payload() + 1, payloadSize() - 1)
        : BufferRef(payload(), payloadSize());
  }

  BufferRef padding() const {
    return isPadded()
        ? BufferRef(payload() + payloadSize() - paddingSize(), paddingSize())
        : BufferRef();
  }

 protected:
  uint8_t paddingLength_;
};
#endif

/**
 * Generates HTTP/2 compliant binary frames.
 */
class Generator {
 public:
  /**
   * Initializes the HTTP/2 generator with standard @c SETTINGS parameter.
   */
  explicit Generator(DataChain* sink);

  /**
   * Initializes the HTTP/2 generator with custom @c SETTINGS parameter.
   *
   * @param sink DataChain to serialize binary frames to.
   * @param maxFrameSize initial @c MAX_FRAME_SIZE to honor.
   * @param headerTableSize initial @c HEADER_TABLE_SIZE to honor in HPACK.
   * @param maxHeaderListSize initial @c MAX_HEADER_LIST_SIZE to honor in HPACK.
   */
  Generator(DataChain* sink,
            size_t maxFrameSize,
            size_t headerTableSize,
            size_t maxHeaderListSize);

  /**
   * Updates the maximum frame size in bytes a frame may fill its payload with.
   *
   * @param value frame size in bytes this Generator is allowed to use per
   *              frame.
   *
   * @note This value is excluding the 9 bytes frame header.
   */
  void setMaxFrameSize(size_t value);

  /**
   * Retrieves the frame size in bytes (without the header,
   * that is, only the payload).
   */
  size_t maxFrameSize() const noexcept;

  // (HPACK) HeaderEncoder tweaks
  void setHeaderTableSize(size_t value);
  void setMaxHeaderListSize(size_t value);

  /**
   * Generates the binary frame for a DATA frame and.
   *
   * @param sid the stream ID this data frame belongs to.
   * @param chunk the data payload to be transmitted (whithout padding).
   * @param last whether or not this is the last frame of the stream @p sid.
   */
  void generateData(StreamID sid, const BufferRef& data, bool last);

  /**
   * Generates the binary frame for a DATA frame and.
   *
   * @note If the @p chunk has to be split into multiple @c DATA frames
   *       and if @p chunk owns the its file descriptor, only the last
   *       @c DATA frame will be closing the file descriptor.
   *
   * @param sid the stream ID this data frame belongs to.
   * @param chunk the data payload to be transmitted (whithout padding).
   * @param last whether or not this is the last frame of the stream @p sid.
   */
  void generateData(StreamID sid, FileView&& chunk, bool last);

  /**
   * Generates one @c HEADERS frame and 0 or more @c CONTINUATION frames.
   *
   * @param sid stream identifier
   * @param headers HTTP message headers
   * @param last @c true if no @c DATA frame is following, @c false otherwise.
   */
  void generateHeaders(StreamID sid, const HeaderFieldList& headers,
                       bool last);

  /**
   * Generates one @c PRIORITY frame.
   *
   * @param sid stream identifier
   * @param exclusive is this an exclusive dependancy on @p dependantStreamID ?
   * @param dependantStreamID the stream (ID) the stream @p sid depends on.
   * @param weight bandwidth weight between 1 and 256 (default 16).
   */
  void generatePriority(StreamID sid, bool exclusive,
                        StreamID dependantStreamID, unsigned weight = 16);

  /**
   * Generates one @c RST_STREAM frame.
   */
  void generateResetStream(StreamID sid, ErrorCode errorCode);

  /**
   * Generates one @c SETTINGS frame.
   *
   * @param settings vector of (parameter/value) tuples.
   */
  void generateSettings(
      const std::vector<std::pair<SettingParameter, unsigned>>& settings);

  /**
   * Generates one @c SETTINGS frame to acknowledge the peers @c SETTINGS frame.
   */
  void generateSettingsAck();

  /**
   * Generates one @c PUSH_PROMISE frame with 0 or more @c CONTINUATION frames.
   *
   * @param sid stream identifier this @c PUSH_PROMISE is accociated with.
   * @param psid the push-promise's stream id. Future @c DATA frames must
   *             use this stream identifier as a reference to this push promise.
   * @param info the HTTP message header info
   */
  void generatePushPromise(StreamID sid, StreamID psid,
                           const HttpResponseInfo& info);

  // =========================================================================

  /**
   * Generates one @c PING frame.
   *
   * @param payload any custom data to be transmitted as payload.
   */
  void generatePing(uint64_t payload);

  /**
   * Generates one @c PING frame.
   *
   * @param payload any custom data to be transmitted as payload.
   */
  void generatePing(const BufferRef& payload);

  /**
   * Generates one @c PING acknowledge-frame.
   *
   * @param payload any custom data to be transmitted as payload.
   */
  void generatePingAck(const BufferRef& payload);

  /**
   * Generates one @c GO_AWAY frame.
   *
   * @param lastStreamID identifier of last stream that might have been
   *                     processed on the local end.
   * @param errorCode reason why we're sending the @c GO_AWAY frame.
   * @param debugData optional debug data.
   */
  void generateGoAway(StreamID lastStreamID, ErrorCode errorCode,
                      const BufferRef& debugData = BufferRef());

  /**
   * Generates one @c WINDOW_UPDATE frame to update the window size of the given
   * stream @c sid.
   *
   * @param sid the stream that will be granted the window update.
   * @param size window size in bytes to be granted.
   */
  void generateWindowUpdate(StreamID sid, size_t size);

 protected:
  void generateFrameHeader(FrameType frameType, unsigned frameFlags,
                           StreamID streamID, size_t payloadSize);

  void write8(unsigned value);
  void write16(unsigned value);
  void write24(unsigned value);
  void write32(unsigned value);
  void write64(uint64_t value);

 private:
  DataChain* sink_;
  size_t maxFrameSize_;
};

// {{{ inlines
inline size_t Generator::maxFrameSize() const noexcept {
  return maxFrameSize_;
}
// }}}

} // namespace http2
} // namespace http
} // namespace xzero