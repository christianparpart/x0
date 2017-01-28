// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/net/IPAddress.h>

namespace xzero {

class InetAddress {
 public:
  static const int V4 = IPAddress::V4;
  static const int V6 = IPAddress::V6;

  InetAddress();
  InetAddress(const std::string& ipaddr, int port, int family = 0);
  InetAddress(const IPAddress& ipaddr, int port);
  explicit InetAddress(const std::string& spec);
  InetAddress(const InetAddress&) = default;
  InetAddress& operator=(const InetAddress&) = default;

  const IPAddress& ip() const noexcept;
  void setIP(const IPAddress& value);

  int port() const noexcept;
  void setPort(int value);

  int family() const noexcept;

 private:
  IPAddress ipaddress_;
  int port_;
};

} // namespace xzero

#include <xzero/net/InetAddress-inl.h>
