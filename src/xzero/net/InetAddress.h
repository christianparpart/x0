// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/net/IPAddress.h>
#include <fmt/format.h>
#include <optional>

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

// {{{ inlines
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
// }}}

} // namespace xzero

namespace fmt {
  template<>
  struct formatter<xzero::InetAddress> {
    using InetAddress = xzero::InetAddress;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) -> decltype(ctx.begin()) {
      return ctx.begin();
    }

    template <typename FormatContext>
    constexpr auto format(const InetAddress& v, FormatContext& ctx) -> decltype(ctx.begin()) {
      return format_to(ctx.begin(), "{}:{}", v.ip(), v.port());
    }
  };
}

namespace fmt {
  template<>
  struct formatter<std::optional<xzero::InetAddress>> {
    using InetAddress = xzero::InetAddress;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const std::optional<InetAddress>& v, FormatContext &ctx) {
      if (v)
        return format_to(ctx.begin(), "{}:{}", v->ip(), v->port());
      else
        return format_to(ctx.begin(), "NONE");
    }
  };
}

