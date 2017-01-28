// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/http2/StreamID.h>
#include <xzero/http/http2/FrameType.h>
#include <xzero/http/http2/ErrorCode.h>
#include <xzero/http/http2/SettingParameter.h>
#include <xzero/http/hpack/Parser.h>
#include <xzero/http/hpack/DynamicTable.h>
#include <xzero/Buffer.h>
#include <list>
#include <utility>
#include <string>

namespace xzero {
namespace http {
namespace http2 {

class FrameListener;

/**
 * HTTP/2 Frames Parser.
 *
 * This API implements parsing HTTP/2 frames on a single connection,
 */
class Parser {
 public:
  /**
   * Initializes the parser with a FrameListener and default limits as set by
   * RFC 7540.
   */
  explicit Parser(FrameListener* listener);

  /**
   * Initializes the parser for a connection.
   *
   * @param listener
   * @param maxFrameSize customize receiving frame size limit (default: 2^14).
   * @param maxHeaderTableSize customize the initial default decoding table
   *                           size in bytes (default: 4096).
   */
  Parser(FrameListener* listener,
         size_t maxFrameSize,
         size_t maxHeaderTableSize);

  /** Available HTTP/2 Parser States. */
  enum class State {
    ConnectionPreface,
    Framing,
  };

  /**
   * Manually changes the parser state.
   */
  void setState(State newState);

  /**
   * Parses @p chunk of HTTP/2 frames.
   *
   * Only complete frames are consumed and parsed by this function.
   *
   * @return number of successfully parsed bytes.
   */
  size_t parseFragment(const BufferRef& chunk);

  /**
   * Parses a single and complete @p frame.
   */
  void parseFrame(const BufferRef& frame);

  /**
   * Decodes a settings frame payload.
   */
  static ErrorCode decodeSettings(
      const BufferRef& payload,
      std::vector<std::pair<SettingParameter, unsigned long>>* settings,
      std::string* debugData);

 protected:
  void parseData(uint8_t flags, StreamID sid, const BufferRef& payload);
  void parseHeaders(uint8_t flags, StreamID sid, const BufferRef& payload);
  void parsePriority(StreamID sid, const BufferRef& payload);
  void parseResetStream(StreamID sid, const BufferRef& payload);
  void parseSettings(uint8_t flags, StreamID sid, const BufferRef& payload);
  void parsePushPromise(uint8_t flags, StreamID sid, const BufferRef& payload);
  void parsePing(uint8_t flags, StreamID sid, const BufferRef& payload);
  void parseGoAway(uint8_t flags, StreamID sid, const BufferRef& payload);
  void parseWindowUpdate(uint8_t flags, StreamID sid, const BufferRef& payload);
  void parseContinuation(uint8_t flags, StreamID sid, const BufferRef& payload);
  bool verifyPadding(const BufferRef& padding);
  void onRequestBegin();

 private:
  State state_;

  FrameListener* listener_;
  size_t maxFrameSize_;

  size_t maxHeaderTableSize_;
  hpack::DynamicTable headerContext_;
  Buffer pendingHeaders_;

  /** Represents the highest remote-initiated stream that has been initiated. */
  StreamID newestStreamID_;

  FrameType lastFrameType_;
  StreamID lastStreamID_;

  StreamID promisedStreamID_;

  StreamID dependsOnSID_;
  bool isStreamClosed_;
  bool isExclusiveDependency_;
  unsigned streamWeight_;
};

} // namespace http2
} // namespace http
} // namespace xzero
