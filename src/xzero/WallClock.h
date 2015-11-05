/**
 * This file is part of the "libxzero" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *   Copyright (c) 2015 Christian Parpart.
 *
 * libxzero is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#ifndef _libxzero_UTIL_WALLCLOCK_H
#define _libxzero_UTIL_WALLCLOCK_H
#include <stdlib.h>
#include <stdint.h>
#include <xzero/UnixTime.h>

namespace xzero {

class WallClock {
public:
  static UnixTime now();
  static uint64_t unixSeconds();
  static uint64_t getUnixMillis();
  static uint64_t unixMillis();
  static uint64_t getUnixMicros();
  static uint64_t unixMicros();
};

}
#endif
