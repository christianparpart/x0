// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/net/IPAddress.h>
#include <fmt/format.h>

namespace xzero {

/**
 * @brief CIDR network notation object.
 *
 * @see IPAddress
 */
class Cidr {
 public:
  /**
   * @brief Initializes an empty cidr notation.
   *
   * e.g. 0.0.0.0/0
   */
  Cidr() : ipaddr_(), prefix_(0) {}

  /**
   * @brief Initializes this CIDR notation with given IP address and prefix.
   */
  Cidr(const char* ipaddress, size_t prefix)
      : ipaddr_(ipaddress), prefix_(prefix) {}

  /**
   * @brief Initializes this CIDR notation with given IP address and prefix.
   */
  Cidr(const IPAddress& ipaddress, size_t prefix)
      : ipaddr_(ipaddress), prefix_(prefix) {}

  /**
   * @brief Retrieves the address part of this CIDR notation.
   */
  const IPAddress& address() const { return ipaddr_; }

  /**
   * @brief Sets the address part of this CIDR notation.
   */
  bool setAddress(const std::string& text, IPAddress::Family family) {
    return ipaddr_.set(text, family);
  }

  /**
   * @brief Retrieves the prefix part of this CIDR notation.
   */
  size_t prefix() const { return prefix_; }

  /**
   * @brief Sets the prefix part of this CIDR notation.
   */
  void setPrefix(size_t n) { prefix_ = n; }

  /**
   * @brief Retrieves the string-form of this network in CIDR notation.
   */
  std::string str() const;

  /**
   * @brief test whether or not given IP address is inside the network.
   *
   * @retval true it is inside this network.
   * @retval false it is not inside this network.
   */
  bool contains(const IPAddress& ipaddr) const;

  /**
   * @brief compares 2 CIDR notations for equality.
   */
  friend bool operator==(const Cidr& a, const Cidr& b);

  /**
   * @brief compares 2 CIDR notations for inequality.
   */
  friend bool operator!=(const Cidr& a, const Cidr& b);

 private:
  IPAddress ipaddr_;
  size_t prefix_;
};

}  // namespace xzero

namespace std {

template <>
struct hash<xzero::Cidr> {
  size_t operator()(const xzero::Cidr& v) const noexcept {
    // TODO: let it honor IPv6 better
    return static_cast<size_t>(*(uint32_t*)(v.address().data()) + v.prefix());
  }
};

}  // namespace std

namespace fmt {
  template<>
  struct formatter<xzero::Cidr> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const xzero::Cidr& v, FormatContext &ctx) {
      return format_to(ctx.begin(), v.str());
    }
  };
}

