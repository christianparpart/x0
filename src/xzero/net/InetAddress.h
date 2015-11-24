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

class InetAddress {
 public:
  InetAddress();
  InetAddress(const IPAddress& ipaddr, int port);
  InetAddress(const InetAddress&) = default;
  InetAddress& operator=(const InetAddress&) = default;

  const IPAddress& ipaddress() const noexcept;
  int port() const noexcept;

 private:
  IPAddress ipaddress_;
  int port_;
};

} // namespace xzero

#include <xzero/net/InetAddress-inl.h>
