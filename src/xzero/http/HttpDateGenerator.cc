// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpDateGenerator.h>
#include <xzero/UnixTime.h>
#include <cassert>
#include <ctime>

namespace xzero {
namespace http {

HttpDateGenerator::HttpDateGenerator()
  : current_(0.0),
    buffer_(),
    mutex_() {
}

UnixTime HttpDateGenerator::getCurrentTime() {
  return UnixTime::now();
}

void HttpDateGenerator::update() {
  UnixTime now = getCurrentTime();

  if (current_ != now) {
    std::lock_guard<std::mutex> _lock(mutex_);

    std::time_t ts = now.unixtime();
    if (struct tm* tm = std::gmtime(&ts)) {
      char buf[256];

      ssize_t n = std::strftime(buf, sizeof(buf), "%a, %d %b %Y %T GMT", tm);
      if (n != 0) {
        current_ = now;
        buffer_.clear();
        buffer_.push_back(buf, n);
      }
    }
  }
}

void HttpDateGenerator::fill(Buffer* target) {
  assert(target != nullptr);

  update();

  std::lock_guard<std::mutex> _lock(mutex_);
  target->push_back(buffer_);
}

} // namespace http
} // namespace xzero
