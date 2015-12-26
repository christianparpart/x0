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

class Generator {
 public:
  explicit Generator(DataChain* sink);

  Generator(DataChain* sink,
            size_t paddingSize,
            size_t maxFrameSize,
            size_t headerTableSize,
            size_t maxHeaderListSize);

  /**
   * Updates the maximum frame size a frame may fill in bytes.
   */
  void setMaxFrameSize(size_t value);

  void setHeaderTableSize(size_t value);
  void setMaxHeaderListSize(size_t value);

  /**
   * Generates the binary frame for a DATA frame and.
   *
   * @param sid the stream ID this data frame belongs to.
   * @param chunk the data payload to be transmitted (whithout padding).
   * @param last whether or not this is the last frame of the stream @p sid.
   *
   * @return number of bytes taking from @p chunk.
   */
  size_t generateData(StreamID sid, const BufferRef& data, bool last);

  /**
   * Generates the binary frame for a DATA frame and.
   *
   * @param sid the stream ID this data frame belongs to.
   * @param chunk the data payload to be transmitted (whithout padding).
   * @param last whether or not this is the last frame of the stream @p sid.
   *
   * @return number of bytes taking from @p chunk.
   */
  size_t generateData(StreamID sid, FileView&& chunk, bool last);

  void generateHeaders(StreamID sid, const HeaderFieldList& headers,
                       bool last);
  void generatePriority(StreamID sid, bool exclusive,
                        StreamID dependantStreamID, unsigned weight);
  void generatePing(StreamID sid, const BufferRef& payload);
  void generateGoAway(StreamID sid);
  void generateResetStream(StreamID sid, ErrorCode errorCode);

  void generateSettings(
      const std::vector<std::pair<SettingParameter, unsigned>>& settings);
  void generateSettingsAcknowledgement();

  void generatePushPromise(StreamID sid, const HttpResponseInfo&);
  void generateWindowUpdate(StreamID sid, unsigned size);

 protected:
  void generateFrameHeader(FrameType frameType, unsigned frameFlags,
                           StreamID streamID, size_t payloadSize);

  void write32(unsigned value);
  void write24(unsigned value);
  void write16(unsigned value);
  void write8(unsigned value);

 private:
  DataChain* sink_;
  size_t paddingSize_;
  size_t maxFrameSize_;
};

} // namespace http2
} // namespace http
} // namespace xzero
