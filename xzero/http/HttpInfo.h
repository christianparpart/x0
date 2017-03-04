// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/HttpVersion.h>
#include <xzero/http/HeaderFieldList.h>
#include <string>

namespace xzero {
namespace http {

/**
 * Base HTTP Message Info.
 *
 * @see HttpRequestInfo
 * @see HttpResponseInfo
 */
class XZERO_HTTP_API HttpInfo {
 public:
  static constexpr size_t UnknownContentLength = static_cast<size_t>(-1);

  HttpInfo(HttpVersion version, size_t contentLength,
           const HeaderFieldList& headers,
           const HeaderFieldList& trailers);
  HttpInfo(const HttpInfo& other) = default;
  HttpInfo& operator=(const HttpInfo& other) = default;

  /** Retrieves the HTTP message version. */
  HttpVersion version() const noexcept { return version_; }
  void setVersion(HttpVersion version) { version_ = version; }

  /** Retrieves the HTTP response headers. */
  const HeaderFieldList& headers() const noexcept { return headers_; }

  /** Retrieves the HTTP response headers. */
  HeaderFieldList& headers() noexcept { return headers_; }

  bool hasHeader(const std::string& name) const { return headers_.contains(name); }
  const std::string& getHeader(const std::string& name) const { return headers_.get(name); }

  void resetContentLength() noexcept;
  void setContentLength(size_t size) noexcept;
  size_t contentLength() const noexcept { return contentLength_; }
  bool hasContentLength() const noexcept {
    return contentLength_ != UnknownContentLength;
  }

  /** Tests whether HTTP message will send trailers. */
  bool hasTrailers() const noexcept { return !trailers_.empty(); }

  /** Retrieves the HTTP response trailers. */
  const HeaderFieldList& trailers() const noexcept { return trailers_; }
  HeaderFieldList& trailers() noexcept { return trailers_; }

  void setTrailers(const HeaderFieldList& list) { trailers_ = list; }

  void reset();

 protected:
  HttpVersion version_;
  size_t contentLength_;
  HeaderFieldList headers_;
  HeaderFieldList trailers_;
};

inline HttpInfo::HttpInfo(HttpVersion version, size_t contentLength,
                          const HeaderFieldList& headers,
                          const HeaderFieldList& trailers)
    : version_(version),
      contentLength_(contentLength),
      headers_(headers),
      trailers_(trailers) {
  //.
}

inline void HttpInfo::reset() {
  version_ = HttpVersion::UNKNOWN;
  contentLength_ = UnknownContentLength;
  headers_.reset();
  trailers_.reset();
}

inline void HttpInfo::setContentLength(size_t size) noexcept {
  contentLength_ = size;
}

inline void HttpInfo::resetContentLength() noexcept {
  contentLength_ = UnknownContentLength;
}

}  // namespace http
}  // namespace xzero
