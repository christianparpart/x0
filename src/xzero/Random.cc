// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Random.h>
#include <sstream>
#include <inttypes.h>

namespace xzero {

Random::Random() {
  std::random_device r;
  prng_.seed(r());
}

uint64_t Random::random64() {
  for (;;) {
    uint64_t rval = prng_();
    if (rval > 0) {
      return rval;
    }
  }
}

#define UX64 "%08" PRIx64

std::string Random::hex64() {
  uint64_t val = random64();
  char buf[9];
  int n = snprintf(buf, sizeof(buf), UX64, val);
  return std::string(buf, n);
}

std::string Random::hex128() {
  uint64_t val[2];
  val[0] = random64();
  val[1] = random64();

  char buf[17];
  int n = snprintf(buf, sizeof(buf), UX64 UX64, val[0], val[1]);
  return std::string(buf, n);
}

std::string Random::hex256() {
  uint64_t val[4];
  val[0] = random64();
  val[1] = random64();
  val[2] = random64();
  val[3] = random64();

  char buf[33];
  int n = snprintf(buf, sizeof(buf), UX64 UX64 UX64 UX64,
                   val[0], val[1], val[2], val[3]);
  return std::string(buf, n);
}

std::string Random::hex512() {
  uint64_t val[8];
  val[0] = random64();
  val[1] = random64();
  val[2] = random64();
  val[3] = random64();
  val[4] = random64();
  val[5] = random64();
  val[6] = random64();
  val[7] = random64();

  char buf[65];
  int n = snprintf(buf, sizeof(buf),
                  UX64 UX64 UX64 UX64 UX64 UX64 UX64 UX64,
                  val[0], val[1], val[2], val[3],
                  val[4], val[5], val[6], val[7]);
  return std::string(buf, n);
}

std::string Random::alphanumericString(int nchars) {
  static const char base[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  std::stringstream sstr;

  for (int i = 0; i < nchars; ++i)
    sstr << base[prng_() % (sizeof(base) - 1)];

  return sstr.str();
}

}  // namespace xzero
