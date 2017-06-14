// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_IPAddress_h
#define x0_IPAddress_h

#include <xzero/Api.h>
#include <xzero/StringUtil.h>
#include <xzero/Option.h>

#include <functional>  // hash<>
#include <stdint.h>
#include <string>
#include <string.h>      // memset()
#include <netinet/in.h>  // in_addr, in6_addr
#include <arpa/inet.h>   // ntohl(), htonl()
#include <stdio.h>
#include <stdlib.h>

namespace xzero {

/**
 * IPv4 or IPv6 network address.
 */
class XZERO_BASE_API IPAddress {
 public:
  static const int V4 = AF_INET;
  static const int V6 = AF_INET6;

 private:
  int family_;
  mutable char cstr_[INET6_ADDRSTRLEN];
  uint8_t buf_[sizeof(struct in6_addr)];

 public:
  IPAddress();
  explicit IPAddress(const in_addr* saddr);
  explicit IPAddress(const in6_addr* saddr);
  explicit IPAddress(const sockaddr_in* saddr);
  explicit IPAddress(const sockaddr_in6* saddr);
  explicit IPAddress(const std::string& text, int family = 0);

  IPAddress& operator=(const std::string& value);
  IPAddress& operator=(const IPAddress& value);

  bool set(const std::string& text, int family);

  void clear();

  int family() const;
  const void* data() const;
  size_t size() const;
  std::string str() const;
  const char* c_str() const;

  friend bool operator==(const IPAddress& a, const IPAddress& b);
  friend bool operator!=(const IPAddress& a, const IPAddress& b);
};

// {{{ impl
inline IPAddress::IPAddress() {
  family_ = 0;
  cstr_[0] = '\0';
  memset(buf_, 0, sizeof(buf_));
}

inline IPAddress::IPAddress(const in_addr* saddr) {
  family_ = AF_INET;
  cstr_[0] = '\0';
  memcpy(buf_, saddr, sizeof(*saddr));
}

inline IPAddress::IPAddress(const in6_addr* saddr) {
  family_ = AF_INET6;
  cstr_[0] = '\0';
  memcpy(buf_, saddr, sizeof(*saddr));
}

inline IPAddress::IPAddress(const sockaddr_in* saddr) {
  family_ = AF_INET;
  cstr_[0] = '\0';
  memcpy(buf_, &saddr->sin_addr, sizeof(saddr->sin_addr));
}

inline IPAddress::IPAddress(const sockaddr_in6* saddr) {
  family_ = AF_INET6;
  cstr_[0] = '\0';
  memcpy(buf_, &saddr->sin6_addr, sizeof(saddr->sin6_addr));
}

// I suggest to use a very strict IP filter to prevent spoofing or injection
inline IPAddress::IPAddress(const std::string& text, int family) {
  if (family != 0) {
    set(text, family);
    // You should use regex to parse ipv6 :) ( http://home.deds.nl/~aeron/regex/
    // )
  } else if (text.find(':') != std::string::npos) {
    set(text, AF_INET6);
  } else {
    set(text, AF_INET);
  }
}

inline IPAddress& IPAddress::operator=(const std::string& text) {
  // You should use regex to parse ipv6 :) ( http://home.deds.nl/~aeron/regex/ )
  if (text.find(':') != std::string::npos) {
    set(text, AF_INET6);
  } else {
    set(text, AF_INET);
  }
  return *this;
}

inline IPAddress& IPAddress::operator=(const IPAddress& v) {
  family_ = v.family_;
  strncpy(cstr_, v.cstr_, sizeof(cstr_));
  memcpy(buf_, v.buf_, v.size());

  return *this;
}

inline bool IPAddress::set(const std::string& text, int family) {
  family_ = family;
  int rv = inet_pton(family, text.c_str(), buf_);
  if (rv <= 0) {
    if (rv < 0)
      perror("inet_pton");
    else
      fprintf(stderr, "IP address Not in presentation format: %s\n",
              text.c_str());

    cstr_[0] = 0;
    return false;
  }
  strncpy(cstr_, text.c_str(), sizeof(cstr_));
  return true;
}

inline void IPAddress::clear() {
  family_ = 0;
  cstr_[0] = 0;
  memset(buf_, 0, sizeof(buf_));
}

inline int IPAddress::family() const { return family_; }

inline const void* IPAddress::data() const { return buf_; }

inline size_t IPAddress::size() const {
  return family_ == V4 ? sizeof(in_addr) : sizeof(in6_addr);
}

inline std::string IPAddress::str() const { return c_str(); }

inline const char* IPAddress::c_str() const {
  if (*cstr_ == '\0') {
    inet_ntop(family_, &buf_, cstr_, sizeof(cstr_));
  }
  return cstr_;
}

inline bool operator==(const IPAddress& a, const IPAddress& b) {
  if (&a == &b) return true;

  if (a.family() != b.family()) return false;

  switch (a.family()) {
    case AF_INET:
    case AF_INET6:
      return memcmp(a.data(), b.data(), a.size()) == 0;
    default:
      return false;
  }

  return false;
}

inline bool operator!=(const IPAddress& a, const IPAddress& b) {
  return !(a == b);
}
// }}}

template<>
inline std::string StringUtil::toString(const IPAddress& ipaddr) {
  return ipaddr.str();
}

template<>
inline std::string StringUtil::toString(IPAddress& ipaddr) {
  return ipaddr.str();
}

template<>
inline std::string StringUtil::toString(IPAddress ipaddr) {
  return ipaddr.str();
}

template<>
inline std::string StringUtil::toString(Option<IPAddress> ipaddr) {
  return ipaddr.isEmpty() ? "None" : ipaddr.get().str();
}

}  // namespace xzero

namespace std {

template <>
struct hash<::xzero::IPAddress>
    : public unary_function<::xzero::IPAddress, size_t> {
  size_t operator()(const ::xzero::IPAddress& v) const {
    return *(uint32_t*)(v.data());
  }
};

inline ostream& operator<<(ostream& os, const ::xzero::IPAddress& ipaddr) {
  os << ipaddr.str();
  return os;
}

} // namespace std

#endif
