// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/HttpInfo.h>
#include <xzero/http/HttpStatus.h>
#include <string>

namespace xzero {
namespace http {

/**
 * HTTP Response Message Info.
 */
class XZERO_HTTP_API HttpResponseInfo : public HttpInfo {
 public:
  HttpResponseInfo();
  HttpResponseInfo(HttpResponseInfo&& other);
  HttpResponseInfo(const HttpResponseInfo& other) = default;
  HttpResponseInfo& operator=(HttpResponseInfo&& other);
  HttpResponseInfo& operator=(const HttpResponseInfo& other) = default;

  HttpResponseInfo(HttpVersion version, HttpStatus status,
                   const std::string& reason, bool isHeadResponse,
                   size_t contentLength,
                   const HeaderFieldList& headers,
                   const HeaderFieldList& trailers);

  /** Retrieves the HTTP response status code. */
  HttpStatus status() const noexcept { return status_; }

  void setStatus(HttpStatus status) { status_ = status; }

  const std::string& reason() const noexcept { return reason_; }

  void setReason(const std::string& text) { reason_ = text; }

  /** Retrieves whether this is an HTTP response to a HEAD request. */
  bool isHeadResponse() const noexcept { return isHeadResponse_; }
  void setIsHeadResponse(bool value) { isHeadResponse_ = value; }

  void reset();

 private:
  HttpStatus status_;
  std::string reason_;
  bool isHeadResponse_;
};

inline HttpResponseInfo::HttpResponseInfo()
    : HttpResponseInfo(HttpVersion::UNKNOWN, HttpStatus::Undefined, "", false,
                       UnknownContentLength, {}, {}) {
}

inline HttpResponseInfo::HttpResponseInfo(HttpResponseInfo&& other)
  : HttpResponseInfo(other.version_, other.status_, "", other.isHeadResponse_,
                     other.contentLength_, {}, {}) {

  reason_.swap(other.reason_);
  headers_.swap(other.headers_);
  trailers_.swap(other.trailers_);
  other.contentLength_ = UnknownContentLength;
}

inline HttpResponseInfo& HttpResponseInfo::operator=(HttpResponseInfo&& other) {
  version_ = other.version_;
  contentLength_ = other.contentLength_;
  headers_ = std::move(other.headers_);
  trailers_ = std::move(other.trailers_);

  status_ = other.status_;
  reason_ = std::move(other.reason_);
  isHeadResponse_ = other.isHeadResponse_;

  return *this;
}

inline HttpResponseInfo::HttpResponseInfo(HttpVersion version,
                                          HttpStatus status,
                                          const std::string& reason,
                                          bool isHeadResponse,
                                          size_t contentLength,
                                          const HeaderFieldList& headers,
                                          const HeaderFieldList& trailers)
    : HttpInfo(version, contentLength, headers, trailers),
      status_(status),
      reason_(reason),
      isHeadResponse_(isHeadResponse) {
  //.
}

inline void HttpResponseInfo::reset() {
  HttpInfo::reset();
  status_ = HttpStatus::Undefined;
  reason_.clear();
  isHeadResponse_ = false;
}

}  // namespace http
}  // namespace xzero
