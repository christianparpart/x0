// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
