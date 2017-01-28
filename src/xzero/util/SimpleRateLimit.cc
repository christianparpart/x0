// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/util/SimpleRateLimit.h>
#include <xzero/MonotonicTime.h>
#include <xzero/MonotonicClock.h>

namespace xzero {
namespace util {

SimpleRateLimit::SimpleRateLimit(
    const Duration& period) :
    period_micros_(period.microseconds()),
    last_micros_(0) {}

bool SimpleRateLimit::check() {
  MonotonicTime now = MonotonicClock::now();

  if (now.microseconds() - last_micros_ >= period_micros_) {
    last_micros_ = now.microseconds();
    return true;
  } else {
    return false;
  }
}

SimpleRateLimitedFn::SimpleRateLimitedFn(
    const Duration& period,
    std::function<void ()> fn) :
    limit_(period),
    fn_(fn) {}

void SimpleRateLimitedFn::runMaybe() {
  if (limit_.check()) {
    fn_();
  }
}

void SimpleRateLimitedFn::runForce() {
  fn_();
}

}
}

