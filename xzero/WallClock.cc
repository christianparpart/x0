// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <sys/time.h>
#include <xzero/WallClock.h>

namespace xzero {

UnixTime WallClock::now() {
  return UnixTime(WallClock::getUnixMicros());
}

uint64_t WallClock::unixSeconds() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return tv.tv_sec;
}

uint64_t WallClock::getUnixMillis() {
  return unixMillis();
}

uint64_t WallClock::unixMillis() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000llu + tv.tv_usec / 1000llu;
}

uint64_t WallClock::getUnixMicros() {
  return unixMicros();
}

uint64_t WallClock::unixMicros() {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000llu + tv.tv_usec;
}

}
