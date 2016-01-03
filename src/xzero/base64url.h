// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
