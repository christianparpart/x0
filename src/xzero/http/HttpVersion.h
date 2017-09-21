// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <string>
#include <iosfwd>

namespace xzero {
namespace http {

/**
 * HTTP protocol version number.
 */
enum class HttpVersion {
  UNKNOWN = 0,
  VERSION_0_9 = 0x09,
  VERSION_1_0 = 0x10,
  VERSION_1_1 = 0x11,
  VERSION_2_0 = 0x20,
};

XZERO_HTTP_API const std::string& as_string(HttpVersion value);
XZERO_HTTP_API std::ostream& operator<<(std::ostream& os, HttpVersion version);
XZERO_HTTP_API HttpVersion make_version(const std::string& value);

} // namespace http
} // namespace xzero
