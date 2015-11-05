// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
