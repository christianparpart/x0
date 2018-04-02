// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/FrameType.h>
#include <xzero/StringUtil.h>
#include <stdio.h>

namespace xzero::http::http2 {

std::string as_string(FrameType type) {
  switch (type) {
    case FrameType::Data:
      return "DATA";
    case FrameType::Headers:
      return "HEADERS";
    case FrameType::Priority:
      return "PRIORITY";
    case FrameType::ResetStream:
      return "RST_STREAM";
    case FrameType::Settings:
      return "SETTINGS";
    case FrameType::PushPromise:
      return "PUSH_PROMIS";
    case FrameType::Ping:
      return "PING";
    case FrameType::GoAway:
      return "GO_AWAY";
    case FrameType::WindowUpdate:
      return "WINDOW_UPDATE";
    case FrameType::Continuation:
      return "CONTINUATION";
    default: {
      char buf[128];
      int n = snprintf(buf, sizeof(buf), "FRAME_TYPE_%u", (unsigned) type);
      return std::string(buf, n);
    }
  }
}

} // namespace xzero::http::http2
