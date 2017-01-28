// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <stdint.h>
#include <string>

#include <xzero/Api.h>

namespace xzero {
namespace hash {

/**
 * This implements the FNV1a (Fowler–Noll–Vo) hash function.
 *
 * @see http://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function
 */
template<typename T>
class XZERO_BASE_API FNV {
 public:
  FNV();
  FNV(T basis, T prime) : basis_(basis), prime_(prime) {}

  T hash(const uint8_t* data, size_t len) const {
    T memory = basis_;

    for (size_t i = 0; i < len; ++i) {
      memory ^= data[i];
      memory *= prime_;
    }

    return memory;
  }

  inline T hash(const void* data, size_t len) const {
    return hash(static_cast<const uint8_t*>(data), len);
  }

  inline T hash(const std::string& data) const {
    return hash(reinterpret_cast<const uint8_t*>(data.data()), data.length());
  }

 protected:
  T basis_;
  T prime_;
};

}  // namespace xzero
}  // namespace hash
