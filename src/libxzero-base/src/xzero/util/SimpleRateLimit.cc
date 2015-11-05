// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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

