// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <cstring>
#include <format>
#include <system_error>

template <>
struct std::formatter<std::errc> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const std::errc& v, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{}", strerror((int)v));
  }
};

template <>
struct std::formatter<std::error_code> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const std::error_code& v, std::format_context& ctx) const {
    return std::format_to(ctx.out(), "{}: {}", v.category().name(),
                          v.message());
  }
};
