// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/DnsClient.h>
#include <xzero/net/IPAddress.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <vector>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>

namespace xzero {

const std::vector<IPAddress>& DnsClient::ipv4(const std::string& name) {
  return lookupIP<sockaddr_in, AF_INET>(name, &ipv4_, &ipv4Mutex_);
}

const std::vector<IPAddress>& DnsClient::ipv6(const std::string& name) {
  return lookupIP<sockaddr_in6, AF_INET6>(name, &ipv6_, &ipv6Mutex_);
}

std::vector<IPAddress> DnsClient::ip(const std::string& name) {
  std::vector<IPAddress> result;
  try {
    const std::vector<IPAddress>& v4 = ipv4(name);
    for (const IPAddress& ip: v4) {
      result.push_back(ip);
    }
  } catch (...) {
  }

  try {
    const std::vector<IPAddress>& v6 = ipv6(name);
    for (const IPAddress& ip: v6) {
      result.push_back(ip);
    }
  } catch (...) {
  }

  if (result.empty())
    RAISE_STATUS(ResolveError);

  return result;
}

class GaiErrorCategory : public std::error_category {
 public:
  static std::error_category& get() {
    static GaiErrorCategory gaiErrorCategory_;
    return gaiErrorCategory_;
  }

  const char* name() const noexcept override {
    return "gai";
  }

  std::string message(int ec) const override {
    return gai_strerror(ec);
  }
};

template<typename InetType, const int AddressFamilty>
const std::vector<IPAddress>& DnsClient::lookupIP(
    const std::string& name,
    std::unordered_map<std::string, std::vector<IPAddress>>* cache,
    std::mutex* cacheMutex) {
  std::lock_guard<decltype(*cacheMutex)> _lk(*cacheMutex);
  auto i = cache->find(name);
  if (i != cache->end())
      return i->second;

  addrinfo* res = nullptr;
  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = 0;
  hints.ai_family = AddressFamilty;
  hints.ai_socktype = SOCK_STREAM; // any, actually

  int rc = getaddrinfo(name.c_str(), nullptr, &hints, &res);
  if (rc != 0)
    RAISE_CATEGORY(rc, GaiErrorCategory::get());

  std::vector<IPAddress> list;

  for (addrinfo* ri = res; ri != nullptr; ri = ri->ai_next)
    list.emplace_back(reinterpret_cast<InetType*>(ri->ai_addr));

  freeaddrinfo(res);

  return (*cache)[name] = list;
}

std::vector<std::string> DnsClient::txt(const std::string& name) {
  // http://stackoverflow.com/questions/2315504/best-way-to-resolve-a-dns-txt-record-on-linux-unix-posix-bsd-type-systems
  RAISE_STATUS(NotImplementedError);
}

std::vector<std::pair<int, std::string>> DnsClient::mx(const std::string& name) {
  RAISE_STATUS(NotImplementedError);
}

// TODO http://stackoverflow.com/questions/9507054/why-srv-res-query-always-returns-1
#if 0
std::vector<DnsClient::SRV> DnsClient::srv(const std::string& service,
                                           const std::string& protocol,
                                           const std::string& name) {
  std::vector<SRV> result;

  std::string name = StringUtil::format("_$0._$1.$2", service, protocol, name);

  // https://gist.github.com/scaryghost/6565447
  int ec = res_query(name.c_str(), C_IN, ns_t_srv, buf, sizeof(buf));
  if (ec < 0)
    return result;

  ns_msg msg;

  return result;
}
#endif

void DnsClient::clearIPv4() {
  std::lock_guard<std::mutex> _lk(ipv4Mutex_);
  ipv4_.clear();
}

void DnsClient::clearIPv6() {
  std::lock_guard<std::mutex> _lk(ipv6Mutex_);
  ipv6_.clear();
}

void DnsClient::clearIP() {
  clearIPv4();
  clearIPv6();
}

}  // namespace xzero
