// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/StreamState.h>
#include <xzero/StringUtil.h>
#include <stdio.h>

namespace xzero {

template<>
std::string StringUtil::toString(http::http2::StreamState value) {
  switch (value) {
    case http::http2::StreamState::Idle: return "Idle";
    case http::http2::StreamState::Open: return "Open";
    case http::http2::StreamState::ReservedRemote: return "ReservedRemote";
    case http::http2::StreamState::ReservedLocal: return "ReservedLocal";
    case http::http2::StreamState::HalfClosedRemote: return "HalfClosedRemote";
    case http::http2::StreamState::HalfClosedLocal: return "HalfClosedLocal";
    case http::http2::StreamState::Closed: return "Closed";
    default: {
      char buf[128];
      int n = snprintf(buf, sizeof(buf), "StreamState_%u",
                       static_cast<unsigned>(value));
      return std::string(buf, n);
    }
  }
}

} // namespace xzero
