// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/RuntimeError.h>
#include <xzero/StringUtil.h>
#include <xzero/Option.h>

namespace xzero {

InetAddress::InetAddress()
    : ipaddress_(),
      port_(0) {
}

InetAddress::InetAddress(const std::string& spec) : InetAddress() {
  size_t n = spec.rfind(':');
  if (n == std::string::npos)
    RAISE(RuntimeError, "Invalid InetAddress argument. Missing port.");

  setIP(IPAddress(spec.substr(0, n)));
  setPort(std::stoi(spec.substr(n + 1)));
}

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
    return StringUtil::format("$0", addr.get());
  } else {
    return "NULL";
  }
}

} // namespace xzero
