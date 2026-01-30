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

template<>
struct std::formatter<xzero::http::http2::StreamState> {
  using StreamState = xzero::http::http2::StreamState;

  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const StreamState& v, std::format_context& ctx) const {
    switch (v) {
      case StreamState::Idle: return std::format_to(ctx.out(), "Idle");
      case StreamState::Open: return std::format_to(ctx.out(), "Open");
      case StreamState::ReservedRemote: return std::format_to(ctx.out(), "ReservedRemote");
      case StreamState::ReservedLocal: return std::format_to(ctx.out(), "ReservedLocal");
      case StreamState::HalfClosedRemote: return std::format_to(ctx.out(), "HalfClosedRemote");
      case StreamState::HalfClosedRemote: return std::format_to(ctx.out(), "HalfClosedRemote");
      case StreamState::Closed: return std::format_to(ctx.out(), "Closed");
      default: return std::format_to(ctx.out(), "({})", (int) v);
    }
  }
};
