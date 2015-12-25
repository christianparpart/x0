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
