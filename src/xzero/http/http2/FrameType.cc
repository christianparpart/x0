// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/FrameType.h>
#include <xzero/StringUtil.h>
#include <stdio.h>

namespace xzero {

template<>
std::string StringUtil::toString(http::http2::FrameType value) {
  switch (value) {
    case http::http2::FrameType::Data:
      return "DATA";
    case http::http2::FrameType::Headers:
      return "HEADERS";
    case http::http2::FrameType::Priority:
      return "PRIORITY";
    case http::http2::FrameType::ResetStream:
      return "RST_STREAM";
    case http::http2::FrameType::Settings:
      return "SETTINGS";
    case http::http2::FrameType::PushPromise:
      return "PUSH_PROMIS";
    case http::http2::FrameType::Ping:
      return "PING";
    case http::http2::FrameType::GoAway:
      return "GO_AWAY";
    case http::http2::FrameType::WindowUpdate:
      return "WINDOW_UPDATE";
    case http::http2::FrameType::Continuation:
      return "CONTINUATION";
    default: {
      char buf[128];
      int n = snprintf(buf, sizeof(buf), "FRAME_TYPE_%u", (unsigned) value);
      return std::string(buf, n);
    }
  }
}

} // namespace xzero
