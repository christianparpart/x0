// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>

namespace xzero {
namespace http {
namespace hpack {

class Huffman {
 public:
  static std::string encode(const std::string& value);
  static std::string decode(const uint8_t* pos, const uint8_t* end);
  static void decode(std::string* output, const uint8_t* pos, const uint8_t* end);

  static size_t encodeLength(const std::string& value);
};

} // namespace hpack
} // namespace http
} // namespace xzero
