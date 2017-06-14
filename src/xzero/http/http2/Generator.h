// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/http2/StreamID.h>
#include <xzero/http/http2/FrameType.h>
#include <xzero/http/http2/SettingParameter.h>
#include <xzero/http/hpack/Generator.h>
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
class HttpRequestInfo;

namespace http2 {

enum class ErrorCode;

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
   * Generates the client-side HTTP/2 connection preface.
   *
   * The client must be sent this always as the first data in an
   * HTTP/2 connection.
   *
   * However, the client may already start sending the actual framed messages
   * without waiting for the server to respond.
   */
  void generateClientConnectionPreface();

  /**
   * Generates the binary frame for a DATA frame.
   *
   * @param sid the stream ID this data frame belongs to.
   * @param chunk the data payload to be transmitted (whithout padding).
   * @param last whether or not this is the last frame of the stream @p sid.
   */
  void generateData(StreamID sid, const BufferRef& data, bool last);

  /**
   * Generates the binary frame for a DATA frame.
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
   * Generates one @c HEADERS frame and 0 or more @c CONTINUATION frames.
   *
   * @param sid stream identifier
   * @param headers HTTP message headers
   * @param dependsOnSID ID of stream this stream depends on.
   * @param isExclusive if stream dependency is given, indicates if it is
   *                    exclusive.
   * @param weight      weighting when sharing bandwidth with other streams.
   *                    TODO (ensure): A value between 1 and 256.
   * @param last @c true if no @c DATA frame is following, @c false otherwise.
   */
  void generateHeaders(StreamID sid, const HeaderFieldList& headers,
                       StreamID dependsOnSID,
                       bool isExclusive,
                       uint8_t weight,
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
   * @param info the pushed HTTP message request (without body).
   */
  void generatePushPromise(StreamID sid, StreamID psid,
                           const HttpRequestInfo& info);

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
  void generatePingAck(uint64_t payload);
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
  void generateContinuations(StreamID sid, const BufferRef& payload);
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
  hpack::Generator headerGenerator_;
};

// {{{ inlines
inline size_t Generator::maxFrameSize() const noexcept {
  return maxFrameSize_;
}
// }}}

} // namespace http2
} // namespace http
} // namespace xzero
