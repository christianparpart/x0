// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
