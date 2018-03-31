// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/RuntimeError.h>
#include <fmt/format.h>
#include <stdexcept>

namespace xzero {

InetAddress::InetAddress()
    : ipaddress_(),
      port_(0) {
}

InetAddress::InetAddress(const std::string& spec) : InetAddress() {
  size_t n = spec.rfind(':');
  if (n == std::string::npos)
    // ("Invalid InetAddress argument. Missing port.");
    throw std::invalid_argument{"spec"};

  setIP(IPAddress(spec.substr(0, n)));
  setPort(std::stoi(spec.substr(n + 1)));
}

std::ostream& operator<<(std::ostream& os, const InetAddress& addr) {
  return os << fmt::format("{}:{}", addr.ip(), addr.port());
}

std::ostream& operator<<(std::ostream& os, const std::optional<InetAddress>& addr) {
  if (addr) {
    return os << fmt::format("{}", addr.value());
  } else {
    return os << "NONE";
  }
}

} // namespace xzero
