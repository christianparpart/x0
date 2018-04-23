// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace xzero {
namespace http {
namespace http2 {

enum class StreamState {
  Idle,
  Open,
  ReservedRemote,
  ReservedLocal,
  HalfClosedRemote,
  HalfClosedLocal,
  Closed,
};

} // namespace http2
} // namespace http
} // namespace xzero

namespace fmt {
  template<>
  struct formatter<xzero::http::http2::StreamState> {
    using StreamState = xzero::http::http2::StreamState;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const StreamState& v, FormatContext &ctx) {
      switch (v) {
        case StreamState::Idle: return format_to(ctx.begin(), "Idle");
        case StreamState::Open: return format_to(ctx.begin(), "Open");
        case StreamState::ReservedRemote: return format_to(ctx.begin(), "ReservedRemote");
        case StreamState::ReservedLocal: return format_to(ctx.begin(), "ReservedLocal");
        case StreamState::HalfClosedRemote: return format_to(ctx.begin(), "HalfClosedRemote");
        case StreamState::HalfClosedRemote: return format_to(ctx.begin(), "HalfClosedRemote");
        case StreamState::Closed: return format_to(ctx.begin(), "Closed");
        default: return format_to(ctx.begin(), "({})", (int) v);
      }
    }
  };
}
