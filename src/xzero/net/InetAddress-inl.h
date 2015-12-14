// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
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
