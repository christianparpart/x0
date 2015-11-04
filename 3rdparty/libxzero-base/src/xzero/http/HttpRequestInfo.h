// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/sysconfig.h>
#include <xzero/http/HttpInfo.h>
#include <xzero/http/HttpMethod.h>
#include <string>

namespace xzero {
namespace http {

/**
 * HTTP Request Message Info.
 */
class XZERO_HTTP_API HttpRequestInfo : public HttpInfo {
 public:
  HttpRequestInfo();

  HttpRequestInfo(HttpVersion version, const HttpMethod method,
                  const std::string& entity, size_t contentLength,
                  const HeaderFieldList& headers);

  HttpRequestInfo(HttpVersion version, const std::string& method,
                  const std::string& entity, size_t contentLength,
                  const HeaderFieldList& headers);

  const std::string& method() const XZERO_NOEXCEPT { return method_; }
  const std::string& entity() const XZERO_NOEXCEPT { return entity_; }

 private:
  std::string method_;
  std::string entity_;
};

inline HttpRequestInfo::HttpRequestInfo()
    : HttpRequestInfo(HttpVersion::UNKNOWN, "", "", 0, {}) {
}

inline HttpRequestInfo::HttpRequestInfo(HttpVersion version,
                                        HttpMethod method,
                                        const std::string& entity,
                                        size_t contentLength,
                                        const HeaderFieldList& headers)
    : HttpRequestInfo(version, to_string(method), entity, contentLength, headers) {
}

inline HttpRequestInfo::HttpRequestInfo(HttpVersion version,
                                        const std::string& method,
                                        const std::string& entity,
                                        size_t contentLength,
                                        const HeaderFieldList& headers)
    : HttpInfo(version, contentLength, headers, {}),
      method_(method),
      entity_(entity) {
}

}  // namespace http
}  // namespace xzero
