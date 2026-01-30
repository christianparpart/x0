// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/defines.h>

#include <format>

#include <memory>
#include <vector>
#include <utility>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <iosfwd>

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

std::string as_string(FrameType type);

} // namespace xzero::http::http2

template<>
struct std::formatter<xzero::http::http2::FrameType> {
  using FrameType = xzero::http::http2::FrameType;

  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const FrameType &t, std::format_context& ctx) const {
    switch (t) {
      case FrameType::Data: return std::format_to(ctx.out(), "Data");
      case FrameType::Headers: return std::format_to(ctx.out(), "Headers");
      case FrameType::Priority: return std::format_to(ctx.out(), "Priority");
      case FrameType::ResetStream: return std::format_to(ctx.out(), "ResetStream");
      case FrameType::Settings: return std::format_to(ctx.out(), "Settings");
      case FrameType::PushPromise: return std::format_to(ctx.out(), "PushPromise");
      case FrameType::Ping: return std::format_to(ctx.out(), "Ping");
      case FrameType::GoAway: return std::format_to(ctx.out(), "GoAway");
      case FrameType::WindowUpdate: return std::format_to(ctx.out(), "WindowUpdate");
      case FrameType::Continuation: return std::format_to(ctx.out(), "Continuation");
      default: return std::format_to(ctx.out(), "({})", (int) t);
    }
  }
};

