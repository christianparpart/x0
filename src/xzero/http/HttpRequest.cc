// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpRequest.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.HttpRequest", msg)
# define TRACE(msg...) logTrace("http.HttpRequest", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

HttpRequest::HttpRequest()
    : HttpRequest("", "", HttpVersion::UNKNOWN, false, {}, nullptr) {
  // .
}

HttpRequest::HttpRequest(std::unique_ptr<HttpInput>&& input)
    : HttpRequest("", "", HttpVersion::UNKNOWN, false, {}, std::move(input)) {
  // .
}

HttpRequest::HttpRequest(const std::string& method, const std::string& path,
                         HttpVersion version, bool secure,
                         const HeaderFieldList& headers,
                         std::unique_ptr<HttpInput>&& input)
    : HttpRequestInfo(version, method, path, 0, headers),
      secure_(secure),
      expect100Continue_(false),
      input_(std::move(input)),
      username_() {
  // .
}

void HttpRequest::setRemoteIP(const Option<IPAddress>& ip) {
  remoteIP_ = ip;
}

const Option<IPAddress>& HttpRequest::remoteIP() const {
  return remoteIP_;
}

void HttpRequest::recycle() {
  TRACE("$0 recycle", this);

  HttpRequestInfo::reset();

  secure_ = false;
  host_.clear();
  input_->recycle();
  username_.clear();
}

void HttpRequest::setHost(const std::string& value) {
  host_ = value;
}

}  // namespace http

template<>
std::string StringUtil::toString(http::HttpRequest* value) {
  return StringUtil::format("HttpRequest[$0]", (void*)value);
}

}  // namespace xzero
