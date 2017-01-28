// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace xzero {
namespace http {
namespace http2 {

enum class ErrorCode {
  /**
   * The associated condition is not as a result of an
   * error. For example, a GOAWAY might include this code to indicate
   * graceful shutdown of a connection.
   */
  NoError = 0,

  /**
   * The endpoint detected an unspecific protocol error.
   * This error is for use when a more specific error code is
   * not available.
   */
  ProtocolError = 1,

  /**
   * The endpoint encountered an unexpected internal error.
   */
  InternalError = 2,

  /**
   * The endpoint detected that its peer violated the flow control protocol.
   */
  FlowControlError = 3,

  /**
   * The endpoint sent a SETTINGS frame, but did
   * not receive a response in a timely manner.
   * See Settings Synchronization (Section 6.5.3).
   */
  SettingsTimeout = 4,

  /**
   * The endpoint received a frame after a stream was half closed.
   */
  StreamClosed = 5,

  /**
   * The endpoint received a frame that was larger
   * than the maximum size that it supports.
   */
  FrameSizeError = 6,

  /**
   * The endpoint refuses the stream prior to
   * performing any application processing, see Section 8.1.4 for
   * details.
   */
  RefusedStream = 7,

  /**
   * Used by the endpoint to indicate that the stream is no longer needed.
   */
  Cancel = 8,

  /**
   * The endpoint is unable to maintain the compression context for the
   * connection.
   */
  CompressionError = 9,

  /**
   * The connection established in response to a
   * CONNECT request (Section 8.3) was reset or abnormally closed.
   */
  ConnectError = 10,

  /**
   * The endpoint detected that its peer is
   * exhibiting a behavior over a given amount of time that has caused
   * it to refuse to process further frames.
   */
  EnhanceYourCalm = 11,

  /**
   * The underlying transport has properties that do not meet minimum security
   * requirements (see Section 9.2).
   */
  InadequateSecurity = 12,

  /**
   * The endpoint requires that HTTP/1.1 is used instead of HTTP/2.
   */
  Http11Required = 13,
};

} // namespace http2
} // namespace http
} // namespace xzero
