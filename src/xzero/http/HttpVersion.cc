// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpVersion.h>
#include <xzero/StringUtil.h>
#include <stdexcept>

namespace xzero {

template<>
std::string StringUtil::toString(http::HttpVersion version) {
  return http::to_string(version);
}

namespace http {

#define SRET(slit) { static std::string val(slit); return val; }

const std::string& to_string(HttpVersion version) {
  switch (version) {
    case HttpVersion::VERSION_0_9: SRET("0.9");
    case HttpVersion::VERSION_1_0: SRET("1.0");
    case HttpVersion::VERSION_1_1: SRET("1.1");
    case HttpVersion::VERSION_2_0: SRET("2.0");
    case HttpVersion::UNKNOWN:
    default:
      throw std::runtime_error("Invalid Argument. Invalid HttpVersion value.");
  }
}

HttpVersion make_version(const std::string& value) {
  if (value.size() != 3 || value[1] != '.')
    return HttpVersion::UNKNOWN;

  if (value[0] == '1') {
    if (value[2] == '1') {
      return HttpVersion::VERSION_1_1;
    } else if (value[2] == '0') {
      return HttpVersion::VERSION_1_0;
    } else {
      return HttpVersion::UNKNOWN;
    }
  } else if (value[0] == '0' && value[2] == '9') {
    return HttpVersion::VERSION_0_9;
  } else {
    return HttpVersion::UNKNOWN;
  }
}

} // namespace http
} // namespace xzero
