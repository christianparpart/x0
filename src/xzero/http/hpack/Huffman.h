// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <string>

namespace xzero {
namespace http {
namespace hpack {

class Huffman {
 public:
  static std::string encode(const std::string& value);
  static std::string decode(const std::string& value);

  static size_t encodeLength(const std::string& value);
};

} // namespace hpack
} // namespace http
} // namespace xzero
