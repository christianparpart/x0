// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/base64.h>

namespace xzero {
namespace base64url {
  extern const int indexmap[256];

  template<typename Iterator, typename Output>
  std::string encode(Iterator begin, Iterator end) {
    static constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_";
    return base64::encode(begin, end, alphabet);
  }

  template<typename Iterator, typename Output>
  size_t decode(Iterator begin, Iterator end, Output* output) {
    return base64::decode(begin, end, indexmap, output);
  }
} // namespace base64url
} // namespace xzero

