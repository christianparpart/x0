// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once
#include <limits>
#include <fmt/format.h>

namespace xzero {
namespace http {
namespace http2 {

enum class SettingParameter {
  HeaderTableSize = 1,
  EnablePush = 2,
  MaxConcurrentStreams = 3,
  InitialWindowSize = 4,
  MaxFrameSize = 5,           //!< max frame *payload* size
  MaxHeaderListSize = 6,
};

std::string as_string(http::http2::SettingParameter parameter);

} // namespace http2
} // namespace http
} // namespace xzero

namespace std {
  template<>
  constexpr xzero::http::http2::SettingParameter
      numeric_limits<xzero::http::http2::SettingParameter>::max() noexcept {
    return xzero::http::http2::SettingParameter::MaxHeaderListSize;
  }
}

namespace fmt {
  template<>
  struct formatter<xzero::http::http2::SettingParameter> {
    using SettingParameter = xzero::http::http2::SettingParameter;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const SettingParameter& v, FormatContext &ctx) {
      switch (v) {
        case SettingParameter::HeaderTableSize: return format_to(ctx.begin(), "HeaderTableSize");
        case SettingParameter::EnablePush: return format_to(ctx.begin(), "EnablePush");
        case SettingParameter::MaxConcurrentStreams: return format_to(ctx.begin(), "MaxConcurrentStreams");
        case SettingParameter::InitialWindowSize: return format_to(ctx.begin(), "InitialWindowSize");
        case SettingParameter::MaxFrameSize: return format_to(ctx.begin(), "MaxFrameSize");
        case SettingParameter::MaxHeaderListSize: return format_to(ctx.begin(), "MaxHeaderListSize");
        default: return format_to(ctx.begin(), "({})", (int) v);
      }
    }
  };
}

