// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <string>

namespace xzero {
namespace http {

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
};

XZERO_HTTP_API std::string to_string(HttpMethod value);
XZERO_HTTP_API HttpMethod to_method(const std::string& value);

} // namespace http
} // namespace xzero
