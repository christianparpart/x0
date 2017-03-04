// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
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
  HttpRequestInfo(const HttpRequestInfo&) = default;
  HttpRequestInfo& operator=(const HttpRequestInfo&) = default;

  HttpRequestInfo(HttpVersion version, const HttpMethod method,
                  const std::string& uri, size_t contentLength,
                  const HeaderFieldList& headers);

  HttpRequestInfo(HttpVersion version, const std::string& method,
                  const std::string& uri, size_t contentLength,
                  const HeaderFieldList& headers);

  HttpMethod method() const noexcept { return method_; }
  const std::string& unparsedMethod() const noexcept { return unparsedMethod_; }
  void setMethod(const std::string& value);

  /**
   * Retrieves the Host header (on HTTP/1) or :authority pseudo header on HTTP/2.
   */
  const std::string& authority() const;
  void setAuthority(const std::string& value);

  const std::string& scheme() const;
  void setScheme(const std::string& value);

  bool setUri(const std::string& uri);
  const std::string& unparsedUri() const noexcept { return unparsedUri_; }
  const std::string& path() const noexcept { return path_; }
  const std::string& query() const noexcept { return query_; }
  int directoryDepth() const noexcept { return directoryDepth_; }

  void reset();

 private:
  std::string unparsedMethod_;
  HttpMethod method_;
  std::string unparsedUri_;
  std::string path_;
  std::string query_;
  int directoryDepth_;
};

inline HttpRequestInfo::HttpRequestInfo()
    : HttpRequestInfo(HttpVersion::UNKNOWN, "", "", 0, {}) {
}

inline HttpRequestInfo::HttpRequestInfo(HttpVersion version,
                                        HttpMethod method,
                                        const std::string& uri,
                                        size_t contentLength,
                                        const HeaderFieldList& headers)
    : HttpRequestInfo(version, to_string(method), uri, contentLength, headers) {
}

inline HttpRequestInfo::HttpRequestInfo(HttpVersion version,
                                        const std::string& method,
                                        const std::string& uri,
                                        size_t contentLength,
                                        const HeaderFieldList& headers)
    : HttpInfo(version, contentLength, headers, {}),
      unparsedMethod_(method),
      method_(to_method(method)),
      unparsedUri_(),
      path_(),
      query_(),
      directoryDepth_(0) {
  setUri(uri);
}

}  // namespace http
}  // namespace xzero
