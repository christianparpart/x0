// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include <string>
#include <xzero/Buffer.h>

namespace xzero {
namespace base64 {
  extern const int indexmap[256];

  std::string encode(const std::string& value);

  template<typename Iterator>
  std::string encode(Iterator begin, Iterator end);

  template<typename Iterator, typename Alphabet>
  std::string encode(Iterator begin, Iterator end, Alphabet alphabet);

  size_t decodeLength(const std::string& value);

  template<typename Iterator>
  size_t decodeLength(Iterator begin, Iterator end);

  template<typename Iterator, typename IndexTable>
  size_t decodeLength(Iterator begin, Iterator end, const IndexTable& index);

  std::string decode(const std::string& input);

  template<typename Output>
  size_t decode(const std::string& input, Output output);

  size_t decode(const std::string& input, Buffer* output);

  template<typename Iterator, typename Output>
  size_t decode(Iterator begin, Iterator end, Output output);

  template<typename Iterator, typename IndexTable, typename Output>
  size_t decode(Iterator begin, Iterator end, const IndexTable& alphabet,
                Output output);
} // namespace base64
} // namespace xzero

#include <xzero/base64-inl.h>
