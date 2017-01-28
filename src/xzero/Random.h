// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/RuntimeError.h>
#include <string>
#include <random>

namespace xzero {

/**
 * Random Value Generator.
 */
class XZERO_BASE_API Random {
 public:
  Random();

  /**
   * Return 64 bits of random data
   */
  uint64_t random64();

  /**
   * Return 64 bits of random data as hex chars
   */
  std::string hex64();

  /**
   * Return 128 bits of random data as hex chars
   */
  std::string hex128();

  /**
   * Return 256 bits of random data as hex chars
   */
  std::string hex256();

  /**
   * Return 512 bits of random data as hex chars
   */
  std::string hex512();

  /**
   * Create a new random alphanumeric string
   */
  std::string alphanumericString(int nchars);

 protected:
  std::mt19937_64 prng_;
};

}  // namespace xzero
