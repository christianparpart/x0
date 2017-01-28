// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/base64.h>

namespace xzero {
namespace base64url {
  template<typename Iterator, typename Output>
  std::string encode(Iterator begin, Iterator end);

  template<typename Iterator, typename Output>
  void decode(Iterator begin, Iterator end, Output output);
}
} // namespace xzero

#include <xzero/base64url-inl.h>
