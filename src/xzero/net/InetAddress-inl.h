// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/net/IPAddress.h>

namespace xzero {

inline InetAddress::InetAddress(const std::string& ipaddr, int port, int family)
    : ipaddress_(ipaddr, family), port_(port) {
}

inline InetAddress::InetAddress(const IPAddress& ipaddr, int port)
    : ipaddress_(ipaddr), port_(port) {
}

inline const IPAddress& InetAddress::ip() const noexcept {
  return ipaddress_;
}

inline void InetAddress::setIP(const IPAddress& value) {
  ipaddress_ = value;
}

inline int InetAddress::port() const noexcept {
  return port_;
}

inline void InetAddress::setPort(int value) {
  port_ = value;
}

inline int InetAddress::family() const noexcept {
  return ipaddress_.family();
}

} // namespace xzero
