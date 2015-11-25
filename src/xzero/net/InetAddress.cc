// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#include <xzero/net/InetAddress.h>
#include <xzero/StringUtil.h>
#include <xzero/Option.h>

namespace xzero {

template<>
std::string StringUtil::toString(InetAddress addr) {
  return StringUtil::format("$0:$1", addr.ip(), addr.port());
}

template<>
std::string StringUtil::toString(const InetAddress& addr) {
  return StringUtil::format("$0:$1", addr.ip(), addr.port());
}

template<>
std::string StringUtil::toString(Option<InetAddress> addr) {
  if (addr.isSome()) {
    return StringUtil::format("$0:$1", addr.get());
  } else {
    return "NULL";
  }
}

} // namespace xzero
