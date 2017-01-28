// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/ErrorCode.h>
#include <xzero/StringUtil.h>
#include <string>
#include <stdio.h>

namespace xzero {

template<>
std::string StringUtil::toString(http::http2::ErrorCode ec) {
  switch (ec) {
    case http::http2::ErrorCode::NoError:
      return "NoError";
    case http::http2::ErrorCode::ProtocolError:
      return "ProtocolError";
    case http::http2::ErrorCode::InternalError:
      return "InternalError";
    case http::http2::ErrorCode::FlowControlError:
      return "FlowControlError";
    case http::http2::ErrorCode::SettingsTimeout:
      return "SettingsTimeout";
    case http::http2::ErrorCode::StreamClosed:
      return "StreamClosed";
    case http::http2::ErrorCode::FrameSizeError:
      return "FrameSizeError";
    case http::http2::ErrorCode::RefusedStream:
      return "RefusedStream";
    case http::http2::ErrorCode::Cancel:
      return "Cancel";
    case http::http2::ErrorCode::CompressionError:
      return "CompressionError";
    case http::http2::ErrorCode::ConnectError:
      return "ConnectError";
    case http::http2::ErrorCode::EnhanceYourCalm:
      return "EnhanceYourCalm";
    case http::http2::ErrorCode::InadequateSecurity:
      return "InadequateSecurity";
    case http::http2::ErrorCode::Http11Required:
      return "Http11Required";
    default: {
      char buf[128];
      int n = snprintf(buf, sizeof(buf), "ERROR_CODE_%u",
                       static_cast<unsigned>(ec));
      return std::string(buf, n);
    }
  }
}

} // namespace xzero
