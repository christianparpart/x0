// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <fmt/format.h>
#include <system_error>
#include <cstring>

namespace fmt {
  template<>
  struct formatter<std::errc> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const std::errc& v, FormatContext &ctx) {
      return format_to(ctx.begin(), strerror((int)v));
    }
  };

  template<>
  struct formatter<std::error_code> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const std::error_code& v, FormatContext &ctx) {
      return format_to(ctx.begin(), "{}: {}", v.category().name(), v.message());
    }
  };
}

