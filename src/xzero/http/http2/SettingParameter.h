// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
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
