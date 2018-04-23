// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <functional>     // hash<>
#include <optional>
#include <iostream>
#include <string>
#include <cstring>        // memset()
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <xzero/defines.h>

#include <fmt/format.h>

#if defined(XZERO_OS_WINDOWS)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>  // in_addr, in6_addr
#include <arpa/inet.h>   // ntohl(), htonl()
#endif

namespace xzero {

/**
 * IPv4 or IPv6 network address.
 */
class IPAddress {
 public:
  enum class Family : int {
    V4 = AF_INET,
    V6 = AF_INET6,
  };

 private:
  Family family_;
  mutable char cstr_[INET6_ADDRSTRLEN];
  uint8_t buf_[sizeof(struct in6_addr)];

 public:
  IPAddress();
  explicit IPAddress(const in_addr* saddr);
  explicit IPAddress(const in6_addr* saddr);
  explicit IPAddress(const sockaddr_in* saddr);
  explicit IPAddress(const sockaddr_in6* saddr);
  explicit IPAddress(const std::string& text);
  explicit IPAddress(const std::string& text, Family v);

  IPAddress& operator=(const std::string& value);
  IPAddress& operator=(const IPAddress& value);

  bool set(const std::string& text, Family family);

  void clear();

  Family family() const;
  const void* data() const;
  size_t size() const;
  std::string str() const;
  const char* c_str() const;

  friend bool operator==(const IPAddress& a, const IPAddress& b);
  friend bool operator!=(const IPAddress& a, const IPAddress& b);
};

// {{{ impl
inline IPAddress::IPAddress() {
  family_ = Family::V4;
  cstr_[0] = '\0';
  memset(buf_, 0, sizeof(buf_));
}

inline IPAddress::IPAddress(const in_addr* saddr) {
  family_ = Family::V4;
  cstr_[0] = '\0';
  memcpy(buf_, saddr, sizeof(*saddr));
}

inline IPAddress::IPAddress(const in6_addr* saddr) {
  family_ = Family::V6;
  cstr_[0] = '\0';
  memcpy(buf_, saddr, sizeof(*saddr));
}

inline IPAddress::IPAddress(const sockaddr_in* saddr) {
  family_ = Family::V4;
  cstr_[0] = '\0';
  memcpy(buf_, &saddr->sin_addr, sizeof(saddr->sin_addr));
}

inline IPAddress::IPAddress(const sockaddr_in6* saddr) {
  family_ = Family::V6;
  cstr_[0] = '\0';
  memcpy(buf_, &saddr->sin6_addr, sizeof(saddr->sin6_addr));
}

// I suggest to use a very strict IP filter to prevent spoofing or injection
inline IPAddress::IPAddress(const std::string& text) {
  // You should use regex to parse ipv6 :) ( http://home.deds.nl/~aeron/regex/ )
  if (text.find(':') != std::string::npos) {
    set(text, Family::V6);
  } else {
    set(text, Family::V4);
  }
}

inline IPAddress::IPAddress(const std::string& text, Family version) {
  set(text, version);
}

inline IPAddress& IPAddress::operator=(const std::string& text) {
  // You should use regex to parse ipv6 :) ( http://home.deds.nl/~aeron/regex/ )
  if (text.find(':') != std::string::npos) {
    set(text, Family::V6);
  } else {
    set(text, Family::V4);
  }
  return *this;
}

inline IPAddress& IPAddress::operator=(const IPAddress& v) {
  family_ = v.family_;
#if defined(XZERO_OS_WINDOWS)
  strncpy_s(cstr_, sizeof(cstr_), v.cstr_, sizeof(v.cstr_));
#else
  strncpy(cstr_, v.cstr_, sizeof(cstr_));
#endif
  memcpy(buf_, v.buf_, v.size());

  return *this;
}

inline bool IPAddress::set(const std::string& text, Family family) {
  family_ = family;
  int rv = inet_pton(static_cast<int>(family), text.c_str(), buf_);
  if (rv <= 0) {
    if (rv < 0)
      perror("inet_pton");
    else
      fprintf(stderr, "IP address Not in presentation format: %s\n",
              text.c_str());

    cstr_[0] = 0;
    return false;
  }

#if defined(XZERO_OS_WINDOWS)
  strncpy_s(cstr_, sizeof(cstr_), text.c_str(), text.size());
#else
  strncpy(cstr_, text.c_str(), sizeof(cstr_));
#endif

  return true;
}

inline void IPAddress::clear() {
  family_ = Family::V4;
  cstr_[0] = 0;
  memset(buf_, 0, sizeof(buf_));
}

inline IPAddress::Family IPAddress::family() const {
  return family_;
}

inline const void* IPAddress::data() const {
  return buf_;
}

inline size_t IPAddress::size() const {
  return family_ == Family::V4 ? sizeof(in_addr) : sizeof(in6_addr);
}

inline std::string IPAddress::str() const { return c_str(); }

inline const char* IPAddress::c_str() const {
  if (*cstr_ == '\0') {
    inet_ntop(static_cast<int>(family_), &buf_, cstr_, sizeof(cstr_));
  }
  return cstr_;
}

inline bool operator==(const IPAddress& a, const IPAddress& b) {
  if (&a == &b)
    return true;

  return a.family() == b.family() && memcmp(a.data(), b.data(), a.size()) == 0;
}

inline bool operator!=(const IPAddress& a, const IPAddress& b) {
  return !(a == b);
}

}  // namespace xzero

namespace std {
  template <>
  struct hash<::xzero::IPAddress> {
    size_t operator()(const ::xzero::IPAddress& v) const {
      return *(uint32_t*)(v.data());
    }
  };
} // namespace std

namespace fmt {
  template<>
  struct formatter<xzero::IPAddress> {
    using IPAddress = xzero::IPAddress;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const IPAddress& v, FormatContext &ctx) {
      return format_to(ctx.begin(), v.str());
    }
  };
}

namespace fmt {
  template<>
  struct formatter<std::optional<xzero::IPAddress>> {
    using IPAddress = xzero::IPAddress;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const std::optional<IPAddress>& v, FormatContext &ctx) {
      if (v)
        return format_to(ctx.begin(), v->str());
      else
        return format_to(ctx.begin(), "NONE");
    }
  };
}

