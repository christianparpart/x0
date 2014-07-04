// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_Cidr_h
#define x0_Cidr_h

#include <x0/Api.h>
#include <x0/IPAddress.h>

namespace x0 {

class X0_API Cidr {
 public:
  Cidr() : ipaddr_(), prefix_(0) {}

  explicit Cidr(const IPAddress& ipaddr, size_t prefix)
      : ipaddr_(ipaddr), prefix_(prefix) {}

  const IPAddress& address() const { return ipaddr_; }
  bool setAddress(const std::string& text, size_t family) {
    return ipaddr_.set(text, family);
  }

  size_t prefix() const { return prefix_; }
  void setPrefix(size_t n) { prefix_ = n; }

  std::string str() const;

  bool contains(const IPAddress& ipaddr) const;

  friend X0_API bool operator==(const Cidr& a, const Cidr& b);
  friend X0_API bool operator!=(const Cidr& a, const Cidr& b);

 private:
  IPAddress ipaddr_;
  size_t prefix_;
};

}  // namespace x0

namespace std {
template <>
struct hash<x0::Cidr> : public unary_function<x0::Cidr, size_t> {
  size_t operator()(const x0::Cidr& v) const {
    // TODO: let it honor IPv6 better
    return *(uint32_t*)(v.address().data()) + v.prefix();
  }
};

inline std::ostream& operator<<(std::ostream& os, const x0::Cidr& cidr) {
  os << cidr.str();
  return os;
}
}

#endif
