// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <format>

// WTF MICROSOFT?!? (winnt.h)
#ifdef DELETE
#undef DELETE
#endif

namespace xzero::http {

enum class HttpMethod {
  UNKNOWN_METHOD,
  OPTIONS,
  GET,
  HEAD,
  POST,
  PUT,
  DELETE,
  TRACE,
  CONNECT,

  PROPFIND,
  PROPPATCH,
  MKCOL,
  COPY,
  MOVE,
  LOCK,
  UNLOCK,
};

std::string as_string(HttpMethod value);
HttpMethod to_method(const std::string& value);

} // namespace xzero::http


template<>
struct std::formatter<xzero::http::HttpMethod> {
  using HttpMethod = xzero::http::HttpMethod;

  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const HttpMethod& v, std::format_context& ctx) const {
    switch (v) {
      case HttpMethod::UNKNOWN_METHOD: return std::format_to(ctx.out(), "UNKNOWN_METHOD");
      case HttpMethod::OPTIONS: return std::format_to(ctx.out(), "OPTIONS");
      case HttpMethod::GET: return std::format_to(ctx.out(), "GET");
      case HttpMethod::HEAD: return std::format_to(ctx.out(), "HEAD");
      case HttpMethod::POST: return std::format_to(ctx.out(), "POST");
      case HttpMethod::PUT: return std::format_to(ctx.out(), "PUT");
      case HttpMethod::DELETE: return std::format_to(ctx.out(), "DELETE");
      case HttpMethod::TRACE: return std::format_to(ctx.out(), "TRACE");
      case HttpMethod::CONNECT: return std::format_to(ctx.out(), "CONNECT");
      case HttpMethod::PROPFIND: return std::format_to(ctx.out(), "PROPFIND");
      case HttpMethod::PROPPATCH: return std::format_to(ctx.out(), "PROPPATCH");
      case HttpMethod::MKCOL: return std::format_to(ctx.out(), "MKCOL");
      case HttpMethod::COPY: return std::format_to(ctx.out(), "COPY");
      case HttpMethod::MOVE: return std::format_to(ctx.out(), "MOVE");
      case HttpMethod::LOCK: return std::format_to(ctx.out(), "LOCK");
      case HttpMethod::UNLOCK: return std::format_to(ctx.out(), "UNLOCK");
      default:
        return std::format_to(ctx.out(), "({})", (int) v);
    }
  }
};
