// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/Cookies.h>
#include <xzero/Uri.h>
#include <xzero/StringUtil.h>

namespace xzero {
namespace http {

bool Cookies::getCookie(
    const CookieList& cookies,
    const std::string& key,
    std::string* dst) {
  for (const auto& cookie : cookies) {
    if (cookie.first == key) {
      *dst = cookie.second;
      return true;
    }
  }

  return false;
}

Cookies::CookieList Cookies::parseCookieHeader(const std::string& header_str) {
  std::vector<std::pair<std::string, std::string>> cookies;

  auto begin = header_str.c_str();
  auto end = begin + header_str.length();
  auto cur = begin;
  while (begin < end) {
    for (; begin < end && *begin == ' '; ++begin);
    for (; cur < end && *cur != '='; ++cur);
    auto split = ++cur;
    for (; cur < end && *cur != ';'; ++cur);
    cookies.emplace_back(
        std::make_pair(
            Uri::decode(std::string(begin, split - 1)),
            Uri::encode(std::string(split, cur))));
    begin = ++cur;
  }

  return cookies;
}

std::string Cookies::makeCookie(
    const std::string& key,
    const std::string& value,
    const UnixTime& expire /* = UnixTime::epoch() */,
    const std::string& path /* = "" */,
    const std::string& domain /* = "" */,
    bool secure /* = false */,
    bool httponly /* = false */) {
  auto cookie_str = StringUtil::format(
      "$0=$1",
      Uri::encode(key),
      Uri::encode(value));

  if (path.length() > 0) {
    cookie_str.append(StringUtil::format("; Path=$0", path));
  }

  if (domain.length() > 0) {
    cookie_str.append(StringUtil::format("; Domain=$0", domain));
  }

  if (static_cast<uint64_t>(expire.unixtime()) > 0) {
    cookie_str.append(StringUtil::format("; Expires=$0",
        expire.format("%a, %d-%b-%Y %H:%M:%S UTC")));
  }

  if (httponly) {
    cookie_str.append("; HttpOnly");
  }

  if (secure) {
    cookie_str.append("; Secure");
  }

  return cookie_str;
}

} // namespace http
} // namespace xzero
