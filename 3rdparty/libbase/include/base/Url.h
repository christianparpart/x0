// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#if !defined(sw_x0_url_hpp)
#define sw_x0_url_hpp (1)

#include <base/Api.h>
#include <base/Buffer.h>
#include <string>
#include <unordered_map>
#include <utility>  // std::make_pair()

namespace base {

class BASE_API Url {
 public:
  typedef std::unordered_map<std::string, std::string> ArgsMap;

  static Url parse(const std::string& url);

  Url();
  Url(const Url& url);
  Url(Url&& url);
  ~Url();

  Url& operator=(const Url& url);

  const std::string& protocol() const { return protocol_; }
  const std::string& username() const { return username_; }
  const std::string& password() const { return password_; }
  const std::string& hostname() const { return hostname_; }
  int port() const { return port_; }
  const std::string& path() const { return path_; }
  const std::string& query() const { return query_; }
  const std::string& fragment() const { return fragment_; }

  // helper

  ArgsMap parseQuery() const {
    return parseQuery(query_.data(), query_.data() + query_.size());
  }

  static ArgsMap parseQuery(const std::string& query) {
    return parseQuery(query.data(), query.data() + query.size());
  }
  static std::string decode(const std::string& value) {
    return decode(value.data(), value.data() + value.size());
  }

  static ArgsMap parseQuery(const BufferRef& query) {
    return parseQuery(query.data(), query.data() + query.size());
  }
  static std::string decode(const BufferRef& value) {
    return decode(value.data(), value.data() + value.size());
  }

  static ArgsMap parseQuery(const Buffer& query) {
    return parseQuery(query.data(), query.data() + query.size());
  }
  static std::string decode(const Buffer& value) {
    return decode(value.data(), value.data() + value.size());
  }

  static ArgsMap parseQuery(const char* begin, const char* end);
  static std::string decode(const char* begin, const char* end);

 private:
  std::string protocol_;
  std::string username_;
  std::string password_;
  std::string hostname_;
  int port_;
  std::string path_;
  std::string query_;
  std::string fragment_;
};

BASE_API bool parseUrl(const std::string& spec, std::string& protocol,
                     std::string& hostname, int& port, std::string& path,
                     std::string& query);
BASE_API bool parseUrl(const std::string& spec, std::string& protocol,
                     std::string& hostname, int& port, std::string& path);
BASE_API bool parseUrl(const std::string& spec, std::string& protocol,
                     std::string& hostname, int& port);

// {{{ inline impl
inline std::string Url::decode(const char* begin, const char* end) {
  Buffer sb;

  for (const char* i = begin; i != end; ++i) {
    if (*i == '%') {
      char snum[3] = {*i, *++i, '\0'};
      sb.push_back((char)(strtol(snum, 0, 16) & 0xFF));
    } else if (*i == '+') {
      sb.push_back(' ');
    } else {
      sb.push_back(*i);
    }
  }
  return sb.str();
}

inline Url::ArgsMap Url::parseQuery(const char* begin, const char* end) {
  ArgsMap args;

  for (const char* p = begin; p != end;) {
    size_t len = 0;
    const char* q = p;

    while (q != end && *q && *q != '=' && *q != '&') {
      ++q;
      ++len;
    }

    if (len) {
      auto name = std::make_pair(p, p + len);
      p += *q == '=' ? len + 1 : len;

      len = 0;
      for (q = p; q != end && *q && *q != '&'; ++q, ++len)
        ;

      if (len) {
        auto value = std::make_pair(p, p + len);
        p += len;

        for (; p != end && *p == '&'; ++p)
          ;  // consume '&' chars (usually just one)

        args[decode(name.first, name.second)] =
            decode(value.first, value.second);
      } else {
        if (*p) {
          ++p;
        }

        args[decode(name.first, name.second)] = "";
      }
    } else if (*p) {  // && or ?& or &=
      ++p;
    }
  }

  return std::move(args);
}
// }}}

}  // namespace base

#endif
