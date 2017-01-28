// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/WallClock.h>
#include <xzero/Duration.h>
#include <functional>
#include <string>
#include <stdlib.h>
#include <stdint.h>

namespace xzero {
namespace util {

class XZERO_BASE_API SimpleRateLimit {
 public:
  SimpleRateLimit(const Duration& period);

  bool check();

 protected:
  uint64_t period_micros_;
  uint64_t last_micros_;
};

class XZERO_BASE_API SimpleRateLimitedFn {
 public:
  SimpleRateLimitedFn(const Duration& period, std::function<void ()> fn);

  void runMaybe();
  void runForce();

 protected:
  SimpleRateLimit limit_;
  std::function<void ()> fn_;
};



} // namespace util
} // namespace xzero
