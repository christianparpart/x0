// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once
#include <limits>

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
