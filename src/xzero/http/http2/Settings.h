// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <xzero/http/http2/SettingParameter.h>

namespace xzero {
namespace http {
namespace http2 {

struct Settings {
  unsigned headerTableSize = 4096;
  bool enablePush = true;
  unsigned maxConcurrentStreams = 100;
  unsigned initialWindowSize = 65535;
  unsigned maxFrameSize = 16384; // XXX max allowed <= (2^24 - 1)
  unsigned maxHeaderListSize = 0xffff; // XXX initial value: unlimited
};

} // namespace http2
} // namespace http
} // namespace xzero
