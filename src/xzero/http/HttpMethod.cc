// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpMethod.h>
#include <xzero/StringUtil.h>
#include <unordered_map>
#include <string>

namespace xzero {

template<>
std::string StringUtil::toString(http::HttpMethod value) {
  return http::to_string(value);
}

namespace http {

#define SRET(slit) { static std::string val(slit); return val; }

std::string to_string(HttpMethod value) {
  switch (value) {
    case HttpMethod::UNKNOWN_METHOD: SRET("UNKNOWN_METHOD");
    case HttpMethod::OPTIONS: SRET("OPTIONS");
    case HttpMethod::GET: SRET("GET");
    case HttpMethod::HEAD: SRET("HEAD");
    case HttpMethod::POST: SRET("POST");
    case HttpMethod::PUT: SRET("PUT");
    case HttpMethod::DELETE: SRET("DELETE");
    case HttpMethod::TRACE: SRET("TRACE");
    case HttpMethod::CONNECT: SRET("CONNECT");
    case HttpMethod::PROPFIND: SRET("PROPFIND");
    case HttpMethod::PROPPATCH: SRET("PROPPATCH");
    case HttpMethod::MKCOL: SRET("MKCOL");
    case HttpMethod::COPY: SRET("COPY");
    case HttpMethod::MOVE: SRET("MOVE");
    case HttpMethod::LOCK: SRET("LOCK");
    case HttpMethod::UNLOCK: SRET("UNLOCK");
  }
  SRET("UNDEFINED_METHOD");
}

HttpMethod to_method(const std::string& value) {
  static const std::unordered_map<std::string, HttpMethod> map = {
    { "CONNECT", HttpMethod::CONNECT},
    { "COPY", HttpMethod::COPY },
    { "DELETE", HttpMethod::DELETE },
    { "GET", HttpMethod::GET },
    { "HEAD", HttpMethod::HEAD },
    { "MKCOL", HttpMethod::MKCOL },
    { "MOVE", HttpMethod::MOVE },
    { "OPTIONS", HttpMethod::OPTIONS},
    { "POST", HttpMethod::POST},
    { "PROPFIND", HttpMethod::PROPFIND },
    { "PROPPATCH", HttpMethod::PROPPATCH },
    { "PUT", HttpMethod::PUT },
    { "TRACE", HttpMethod::TRACE },
  };

  const auto i = map.find(value);
  if (i != map.end())
    return i->second;
  return HttpMethod::UNKNOWN_METHOD;
}

} // namespace http
} // namespace xzero
