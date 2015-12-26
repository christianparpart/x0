// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
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

