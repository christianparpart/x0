// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <string>
#include <fmt/format.h>

// WTF MSVC?
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


namespace fmt {
  template<>
  struct formatter<xzero::http::HttpMethod> {
    using HttpMethod = xzero::http::HttpMethod;

    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    constexpr auto format(const HttpMethod& v, FormatContext &ctx) {
      switch (v) {
        case HttpMethod::UNKNOWN_METHOD: return format_to(ctx.begin(), "UNKNOWN_METHOD");
        case HttpMethod::OPTIONS: return format_to(ctx.begin(), "OPTIONS");
        case HttpMethod::GET: return format_to(ctx.begin(), "GET");
        case HttpMethod::HEAD: return format_to(ctx.begin(), "HEAD");
        case HttpMethod::POST: return format_to(ctx.begin(), "POST");
        case HttpMethod::PUT: return format_to(ctx.begin(), "PUT");
        case HttpMethod::DELETE: return format_to(ctx.begin(), "DELETE");
        case HttpMethod::TRACE: return format_to(ctx.begin(), "TRACE");
        case HttpMethod::CONNECT: return format_to(ctx.begin(), "CONNECT");
        case HttpMethod::PROPFIND: return format_to(ctx.begin(), "PROPFIND");
        case HttpMethod::PROPPATCH: return format_to(ctx.begin(), "PROPPATCH");
        case HttpMethod::MKCOL: return format_to(ctx.begin(), "MKCOL");
        case HttpMethod::COPY: return format_to(ctx.begin(), "COPY");
        case HttpMethod::MOVE: return format_to(ctx.begin(), "MOVE");
        case HttpMethod::LOCK: return format_to(ctx.begin(), "LOCK");
        case HttpMethod::UNLOCK: return format_to(ctx.begin(), "UNLOCK");
        default:
          return format_to(ctx.begin(), "({})", (int) v);
      }
    }
  };
}
