// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

namespace xzero {
namespace http {
namespace http2 {

enum class StreamState {
  Idle,
  Open,
  ReservedRemote,
  ReservedLocal,
  HalfClosedRemote,
  HalfClosedLocal,
  Closed,
};

} // namespace http2
} // namespace http
} // namespace xzero
