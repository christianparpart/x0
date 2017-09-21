// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/StreamState.h>
#include <xzero/StringUtil.h>
#include <iostream>
#include <stdio.h>

namespace xzero::http::http2 {

std::ostream& operator<<(std::ostream& os, StreamState value) {
  switch (value) {
    case http::http2::StreamState::Idle: return os << "Idle";
    case http::http2::StreamState::Open: return os << "Open";
    case http::http2::StreamState::ReservedRemote: return os << "ReservedRemote";
    case http::http2::StreamState::ReservedLocal: return os << "ReservedLocal";
    case http::http2::StreamState::HalfClosedRemote: return os << "HalfClosedRemote";
    case http::http2::StreamState::HalfClosedLocal: return os << "HalfClosedLocal";
    case http::http2::StreamState::Closed: return os << "Closed";
    default: {
      char buf[128];
      int n = snprintf(buf, sizeof(buf), "StreamState_%u",
                       static_cast<unsigned>(value));
      return os << std::string(buf, n);
    }
  }
}

} // namespace xzero::http::http2
