// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <cstdint>

namespace xzero {
namespace http {
namespace http2 {

/**
 * Credit based Flow Control API for HTTP/2.
 */
class Flow {
 public:
  /**
   * Maximum number of credits (bytes) a single flow may be charged with in
   * total at single time.
   */
  static const std::size_t MaxValue = (1llu << 31) - 1;

  Flow();

  /**
   * Initializes the flow with given number @p n of credits (bytes) pre-charged.
   *
   * @param n number of credits (bytes) to pre-charge.
   */
  explicit Flow(std::size_t n);

  /**
   * Retrieves the number of credits (bytes) available in this flow.
   */
  std::size_t available() const noexcept;

  /**
   * Charges the flow by given credits (bytes) @p n.
   *
   * @param n number of credits (bytes) to add.
   */
  bool charge(std::size_t n);

  /**
   * Takes off @p n credits from this flow.
   *
   * @param n number of credits (bytes) to (exactly) take.
   */
  void take(std::size_t n);

 private:
  std::size_t credits_;
};

} // namespace http2
} // namespace http
} // namespace xzero

#include <xzero/http/http2/Flow-inl.h>
