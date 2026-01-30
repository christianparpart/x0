// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once
#include <limits>
#include <format>

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

template<>
struct std::formatter<xzero::http::http2::SettingParameter> {
  using SettingParameter = xzero::http::http2::SettingParameter;

  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const SettingParameter& v, std::format_context& ctx) const {
    switch (v) {
      case SettingParameter::HeaderTableSize: return std::format_to(ctx.out(), "HeaderTableSize");
      case SettingParameter::EnablePush: return std::format_to(ctx.out(), "EnablePush");
      case SettingParameter::MaxConcurrentStreams: return std::format_to(ctx.out(), "MaxConcurrentStreams");
      case SettingParameter::InitialWindowSize: return std::format_to(ctx.out(), "InitialWindowSize");
      case SettingParameter::MaxFrameSize: return std::format_to(ctx.out(), "MaxFrameSize");
      case SettingParameter::MaxHeaderListSize: return std::format_to(ctx.out(), "MaxHeaderListSize");
      default: return std::format_to(ctx.out(), "({})", (int) v);
    }
  }
};

