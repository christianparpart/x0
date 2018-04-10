// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/ErrorCode.h>
#include <string>
#include <fmt/format.h>

namespace xzero::http::http2 {

std::string as_string(ErrorCode ec) {
  switch (ec) {
    case ErrorCode::NoError:
      return "NoError";
    case ErrorCode::ProtocolError:
      return "ProtocolError";
    case ErrorCode::InternalError:
      return "InternalError";
    case ErrorCode::FlowControlError:
      return "FlowControlError";
    case ErrorCode::SettingsTimeout:
      return "SettingsTimeout";
    case ErrorCode::StreamClosed:
      return "StreamClosed";
    case ErrorCode::FrameSizeError:
      return "FrameSizeError";
    case ErrorCode::RefusedStream:
      return "RefusedStream";
    case ErrorCode::Cancel:
      return "Cancel";
    case ErrorCode::CompressionError:
      return "CompressionError";
    case ErrorCode::ConnectError:
      return "ConnectError";
    case ErrorCode::EnhanceYourCalm:
      return "EnhanceYourCalm";
    case ErrorCode::InadequateSecurity:
      return "InadequateSecurity";
    case ErrorCode::Http11Required:
      return "Http11Required";
    default: {
      return fmt::format("ERROR_CODE_{}", static_cast<unsigned>(ec));
    }
  }
}

} // namespace xzero::http::http2
