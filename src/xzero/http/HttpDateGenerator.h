// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Buffer.h>
#include <xzero/UnixTime.h>
#include <mutex>

namespace xzero {

class WallClock;

namespace http {

/**
 * API to generate an HTTP conform Date response header field value.
 */
class HttpDateGenerator {
 public:
  explicit HttpDateGenerator();

  void update();
  void fill(Buffer* target);

  virtual UnixTime getCurrentTime();

 private:
  UnixTime current_;
  Buffer buffer_;
  std::mutex mutex_;
};

} // namespace http
} // namespace xzero
