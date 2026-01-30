// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <iosfwd>
#include <format>

namespace xzero::http {

/**
 * HTTP protocol version number.
 */
enum class HttpVersion {
  UNKNOWN = 0,
  VERSION_0_9 = 0x09,
  VERSION_1_0 = 0x10,
  VERSION_1_1 = 0x11,
  VERSION_2_0 = 0x20,
};

const std::string& as_string(HttpVersion value);
HttpVersion make_version(const std::string& value);

} // namespace xzero::http

template<>
struct std::formatter<xzero::http::HttpVersion> {
  using HttpVersion = xzero::http::HttpVersion;

  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const HttpVersion& v, std::format_context& ctx) const {
    switch (v) {
      case HttpVersion::VERSION_0_9:
        return std::format_to(ctx.out(), "0.9");
      case HttpVersion::VERSION_1_0:
        return std::format_to(ctx.out(), "1.0");
      case HttpVersion::VERSION_1_1:
        return std::format_to(ctx.out(), "1.1");
      case HttpVersion::VERSION_2_0:
        return std::format_to(ctx.out(), "2.0");
      case HttpVersion::UNKNOWN:
        return std::format_to(ctx.out(), "UNKNOWN");
      default:
        return std::format_to(ctx.out(), "({})", (int) v);
    }
  }
};

