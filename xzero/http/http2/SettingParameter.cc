// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/SettingParameter.h>
#include <xzero/StringUtil.h>
#include <stdio.h>

namespace xzero {

template<>
std::string StringUtil::toString(http::http2::SettingParameter parameter) {
  switch (parameter) {
    case http::http2::SettingParameter::HeaderTableSize:
      return "HeaderTableSize";
    case http::http2::SettingParameter::EnablePush:
      return "EnablePush";
    case http::http2::SettingParameter::MaxConcurrentStreams:
      return "MaxConcurrentStreams";
    case http::http2::SettingParameter::InitialWindowSize:
      return "InitialWindowSize";
    case http::http2::SettingParameter::MaxFrameSize:
      return "MaxFrameSize";
    case http::http2::SettingParameter::MaxHeaderListSize:
      return "MaxHeaderListSize";
    default: {
      char buf[128];
      int n = snprintf(buf, sizeof(buf), "SETTINGS_PARAMETER_%u",
                       static_cast<unsigned>(parameter));
      return std::string(buf, n);
    }
  }
}

} // namespace xzero

