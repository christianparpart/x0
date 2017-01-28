// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

namespace xzero {
namespace http {
namespace http2 {

inline Flow::Flow()
    : Flow(65536) {
}

inline Flow::Flow(std::size_t n)
    : credits_(n) {
}

inline std::size_t Flow::available() const noexcept {
  return credits_;
}

inline bool Flow::charge(std::size_t n) {
  if (n > MaxValue - available())
    return false;

  credits_ += n;
  return true;
}

inline void Flow::take(std::size_t n) {
  if (n > credits_)
    return;

  credits_ -= n;
}

} // namespace http2
} // namespace http
} // namespace xzero
