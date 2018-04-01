// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/SettingParameter.h>
#include <xzero/StringUtil.h>
#include <stdio.h>

namespace xzero::http::http2 {

std::string as_string(http::http2::SettingParameter parameter) {
  switch (parameter) {
    case SettingParameter::HeaderTableSize:
      return "HeaderTableSize";
    case SettingParameter::EnablePush:
      return "EnablePush";
    case SettingParameter::MaxConcurrentStreams:
      return "MaxConcurrentStreams";
    case SettingParameter::InitialWindowSize:
      return "InitialWindowSize";
    case SettingParameter::MaxFrameSize:
      return "MaxFrameSize";
    case SettingParameter::MaxHeaderListSize:
      return "MaxHeaderListSize";
    default: {
      char buf[128];
      int n = snprintf(buf, sizeof(buf), "SETTINGS_PARAMETER_%u",
                       static_cast<unsigned>(parameter));
      return std::string(buf, n);
    }
  }
}

} // namespace xzero::http::http2

