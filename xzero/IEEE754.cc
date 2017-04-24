// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/**
 * This file is part of the "FnordMetric" project
 *   Copyright (c) 2014 Paul Asmuth, Google Inc.
 *
 * FnordMetric is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v3.0. You should have received a
 * copy of the GNU General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */
#include <xzero/IEEE754.h>
#include <xzero/RuntimeError.h>
#include <limits>
#include <cstring>
#include <cstdio>

namespace xzero {

uint64_t IEEE754::toBytes(double value) {
  uint64_t bytes;

  constexpr bool ieee754 =
      std::numeric_limits<double>::is_iec559 &&
      sizeof(double) == sizeof(uint64_t);

  static_assert(
      ieee754,
      "IEEE 754 floating point conversion not yet implemented");

  if (ieee754) {
    memcpy((void *) &bytes, (void *) &value, sizeof(bytes));
  } else {
    /* not yet implemented */
    bytes = 0;
  }

  return bytes;
}

double IEEE754::fromBytes(uint64_t bytes) {
  double value;

  constexpr bool ieee754 =
      std::numeric_limits<double>::is_iec559 &&
      sizeof(double) == sizeof(uint64_t);

  static_assert(
      ieee754,
      "IEEE 754 floating point conversion not yet implemented");

  if (ieee754) {
    memcpy((void *) &value, (void *) &bytes, sizeof(bytes));
  } else {
    /* not yet implemented */
    value = 0;
  }

  return value;
}

} // namespace xzero
