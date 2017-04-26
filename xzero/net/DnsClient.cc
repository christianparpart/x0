// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/DnsClient.h>
#include <xzero/net/IPAddress.h>
#include <xzero/RuntimeError.h>
#include <xzero/MonotonicClock.h>
#include <xzero/logging.h>
#include <vector>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <algorithm>

namespace xzero {

DnsClient::DnsClient()
  : ipv4_(),
    ipv4Mutex_(),
    ipv6_(),
    ipv6Mutex_() {
}

DnsClient::~DnsClient() {
}

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
  if (i != cache->end()) {
    return i->second;
  }

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

std::vector<std::string> DnsClient::txt(const std::string& fqdn) {
  std::lock_guard<decltype(txtCacheMutex_)> _lk(txtCacheMutex_);

  MonotonicTime now = MonotonicClock::now();
  std::vector<TXT>& txtCache = txtCache_[fqdn];
  {
    auto i = txtCache.begin();
    auto e = txtCache.end();
    while (i != e) {
      if (i->ttl < now) {
        txtCache_.clear();
        break;
      } else {
        ++i;
      }
    }
  }

  if (!txtCache.empty()) {
    logDebug("DnsClient", "using cached TXT: $0", fqdn);
    std::vector<std::string> result;
    for (const auto& txt: txtCache) {
      result.emplace_back(txt.text);
    }
    return result;
  }

  logDebug("DnsClient", "resolving TXT: $0", fqdn);
  Buffer answer(NS_MAXMSG);
  int answerLength = res_query(fqdn.c_str(),
                               ns_c_in,
                               ns_t_txt,
                               (unsigned char*) answer.data(),
                               answer.capacity());
  if (answerLength < 0) {
    logDebug("DnsClient", "TXT lookup failed for $0", fqdn);
    return std::vector<std::string>();
  }
  answer.resize(answerLength);

  std::vector<std::string> result;
  ns_msg nsMsg;
  ns_initparse((unsigned char*) answer.data(), answer.size(), &nsMsg);

  // iterate through answer section
  for (unsigned x = 0; x < ns_msg_count(nsMsg, ns_s_an); x++) {
    ns_rr rr;
    ns_parserr(&nsMsg, ns_s_an, x, &rr);

    if (ns_rr_type(rr) == ns_t_txt) {
      MonotonicTime ttl = now + Duration::fromSeconds(ns_rr_ttl(rr));
      std::string text((char*) (ns_rr_rdata(rr) + 1),
                       (size_t) ns_rr_rdata(rr)[0]);

      txtCache.emplace_back(TXT{ .ttl = ttl,
                                 .text = text });
      result.emplace_back(text);
    }
  }

  return result;
}

std::vector<std::pair<int, std::string>> DnsClient::mx(const std::string& name) {
  RAISE_STATUS(NotImplementedError);
}

std::vector<DnsClient::SRV> DnsClient::srv(const std::string& service,
                                           const std::string& protocol,
                                           const std::string& name) {
  return srv(StringUtil::format("_$0._$1.$2.", service, protocol, name));
}

std::vector<DnsClient::SRV> DnsClient::srv(const std::string& fqdn) {
  std::lock_guard<decltype(srvCacheMutex_)> _lk(srvCacheMutex_);
  const MonotonicTime now = MonotonicClock::now();

  // evict dead records
  std::vector<SRV>& srvCache = srvCache_[fqdn];
  {
    auto i = srvCache.begin();
    auto e = srvCache.end();
    while (i != e) {
      if (i->ttl <= now) {
        srvCache.erase(i);
      } else {
        ++i;
      }
    }
  }

  if (!srvCache.empty())
    return srvCache;

  Buffer answer(NS_MAXMSG);
  int answerLength = res_query(fqdn.c_str(),
                               ns_c_in,
                               ns_t_srv,
                               (unsigned char*) answer.data(),
                               answer.capacity());
  if (answerLength < 0) {
    logDebug("DnsClient", "SRV lookup failed for $0", fqdn);
    return std::vector<SRV>();
  }
  answer.resize(answerLength);

  ns_msg nsMsg;
  ns_initparse((unsigned char*) answer.data(), answer.size(), &nsMsg);

  // iterate through answer section
  for (unsigned x = 0; x < ns_msg_count(nsMsg, ns_s_an); x++) {
    ns_rr rr;
    ns_parserr(&nsMsg, ns_s_an, x, &rr);

    if (ns_rr_type(rr) == ns_t_srv) {
      char name[NS_MAXDNAME];
      dn_expand(ns_msg_base(nsMsg),
                ns_msg_end(nsMsg),
                ns_rr_rdata(rr) + 6,
                name,
                sizeof(name));

      srvCache.emplace_back(SRV{
          .ttl = now + Duration::fromSeconds(ns_rr_ttl(rr)),
          .priority = ntohs(*(unsigned short*)ns_rr_rdata(rr)),
          .weight = ntohs(*((unsigned short*)ns_rr_rdata(rr) + 1)),
          .port = ntohs(*((unsigned short*)ns_rr_rdata(rr) + 2)),
          .target = name,
      });
    }
  }

  // iterate through Additional Records
  std::unordered_map<std::string, std::vector<IPAddress>> ipv4;
  for (unsigned x = 0; x < ns_msg_count(nsMsg, ns_s_ar); x++) {
    ns_rr rr;
    ns_parserr(&nsMsg, ns_s_ar, x, &rr);

    if (ns_rr_type(rr) == ns_t_a) {
      // TODO: honor ttl in IPv4 cache
      auto ttl = Duration::fromSeconds(ns_rr_ttl(rr));

      in_addr addr;
      addr.s_addr = *(uint32_t*) ns_rr_rdata(rr);
      IPAddress ip(&addr);
      std::string name = rr.name;

      logDebug("DnsClient", "Additional Section: $0 $1 IN A $2 ($3)",
               name, ttl.seconds(), ip, name.size());

      ipv4[name].emplace_back(ip);
    }
  }

  {
    std::lock_guard<decltype(ipv4Mutex_)> _lk(ipv4Mutex_);
    for (const auto& ip: ipv4) {
      ipv4_[ip.first] = std::move(ip.second);
    }
  }

  return srvCache;
}

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
