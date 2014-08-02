// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_Cidr_h
#define x0_Cidr_h

#include <base/Api.h>
#include <base/IPAddress.h>

namespace base {

/**
 * @brief CIDR network notation object.
 *
 * @see IPAddress
 */
class BASE_API Cidr {
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
  bool setAddress(const std::string& text, size_t family) {
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
  friend BASE_API bool operator==(const Cidr& a, const Cidr& b);

  /**
   * @brief compares 2 CIDR notations for inequality.
   */
  friend BASE_API bool operator!=(const Cidr& a, const Cidr& b);

 private:
  IPAddress ipaddr_;
  size_t prefix_;
};

}  // namespace base

namespace std {

template <>
struct hash<base::Cidr> : public unary_function<base::Cidr, size_t> {
  size_t operator()(const base::Cidr& v) const {
    // TODO: let it honor IPv6 better
    return *(uint32_t*)(v.address().data()) + v.prefix();
  }
};

inline std::ostream& operator<<(std::ostream& os, const base::Cidr& cidr) {
  os << cidr.str();
  return os;
}

}  // namespace std

#endif
