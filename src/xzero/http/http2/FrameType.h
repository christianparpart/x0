// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/defines.h>

#include <fmt/format.h>

#include <memory>
#include <vector>
#include <utility>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <iosfwd>
#include <arpa/inet.h>

namespace xzero::http::http2 {

enum class FrameType {
  Data = 0,
  Headers = 1,
  Priority = 2,
  ResetStream = 3,
  Settings = 4,
  PushPromise = 5,
  Ping = 6,
  GoAway = 7,
  WindowUpdate = 8,
  Continuation = 9,
};

std::ostream& operator<<(std::ostream& os, FrameType type);
std::string as_string(FrameType type);

} // namespace xzero::http::http2

namespace fmt {
  template<>
  struct formatter<xzero::http::http2::FrameType> {
    using FrameType = xzero::http::http2::FrameType;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const FrameType &t, FormatContext &ctx) {
      switch (t) {
        case FrameType::Data: return format_to(ctx.begin(), "Data");
        case FrameType::Headers: return format_to(ctx.begin(), "Headers");
        case FrameType::Priority: return format_to(ctx.begin(), "Priority");
        case FrameType::ResetStream: return format_to(ctx.begin(), "ResetStream");
        case FrameType::Settings: return format_to(ctx.begin(), "Settings");
        case FrameType::PushPromise: return format_to(ctx.begin(), "PushPromise");
        case FrameType::Ping: return format_to(ctx.begin(), "Ping");
        case FrameType::GoAway: return format_to(ctx.begin(), "GoAway");
        case FrameType::WindowUpdate: return format_to(ctx.begin(), "WindowUpdate");
        case FrameType::Continuation: return format_to(ctx.begin(), "Continuation");
        default: return format_to(ctx.begin(), "({})", (int) t);
      }
    }
  };
}

