// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/hash/FNV.h>

namespace xzero {
namespace hash {

/**
 * Parameters for the 32bit version of the FNV hash function
 *
 * basis: 2166136261
 * prime: 16777619
 *
 */
template<> FNV<uint32_t>::FNV()
    : basis_(2166136261llu),
      prime_(16777619llu) {
}


/**
 * Parameters for the 64bit version of the FNV hash function
 *
 * basis: 14695981039346656037
 * prime: 1099511628211
 *
 */
template<> FNV<uint64_t>::FNV()
    : basis_(14695981039346656037llu),
      prime_(1099511628211llu) {
}

}  // namespace hash
}  // namespace xzero
