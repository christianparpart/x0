// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <format>

namespace xzero::http::http2 {

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

std::string as_string(ErrorCode ec);

} // namespace xzero::http::http2

template<>
struct std::formatter<xzero::http::http2::ErrorCode> {
  using ErrorCode = xzero::http::http2::ErrorCode;

  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const ErrorCode& v, std::format_context& ctx) const {
    switch (v) {
      case ErrorCode::NoError: return std::format_to(ctx.out(), "No Error");
      case ErrorCode::ProtocolError: return std::format_to(ctx.out(), "Protocol Error");
      case ErrorCode::InternalError: return std::format_to(ctx.out(), "Internal Error");
      case ErrorCode::FlowControlError: return std::format_to(ctx.out(), "Flow Control Error");
      case ErrorCode::SettingsTimeout: return std::format_to(ctx.out(), "Settings Timeout");
      case ErrorCode::StreamClosed: return std::format_to(ctx.out(), "Stream Closed");
      case ErrorCode::FrameSizeError: return std::format_to(ctx.out(), "Frame Size Error");
      case ErrorCode::RefusedStream: return std::format_to(ctx.out(), "Refused Stream");
      case ErrorCode::Cancel: return std::format_to(ctx.out(), "Cancel");
      case ErrorCode::CompressionError: return std::format_to(ctx.out(), "Compression Error");
      case ErrorCode::ConnectError: return std::format_to(ctx.out(), "Connect Error");
      case ErrorCode::EnhanceYourCalm: return std::format_to(ctx.out(), "Enhance Your Calm");
      case ErrorCode::InadequateSecurity: return std::format_to(ctx.out(), "Inadequate Security");
      case ErrorCode::Http11Required: return std::format_to(ctx.out(), "HTTP/1.1 Required");
      default:
        return std::format_to(ctx.out(), "({})", (int) v);
    }
  }
};

