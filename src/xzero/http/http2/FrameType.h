// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/defines.h>

#include <memory>
#include <vector>
#include <utility>
#include <stdint.h>
#include <unordered_map>
#include <map>
#include <arpa/inet.h>

namespace xzero {
namespace http {
namespace http2 {

enum class FrameType {
  Data = 0,
  Headers = 1,
  Priority = 2,
  ResetStream = 3,
  Settings = 4,
  PushPromise = 5,
  Ping = 6,
  GoAway = 7,
  WindowUpdate = 8,
  Continuation = 9,
};

} // namespace http2
} // namespace http
} // namespace xzero
