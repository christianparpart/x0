// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpRequest.h>
#include <xzero/io/FileUtil.h>
#include <xzero/logging.h>

#include <unistd.h>

namespace xzero {
namespace http {

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.HttpRequest: " msg)
# define TRACE(msg...) logTrace("http.HttpRequest: " msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

HttpRequest::HttpRequest()
    : HttpRequest(HttpVersion::UNKNOWN, "", "", {}, false, {}) {
  // .
}

HttpRequest::HttpRequest(HttpVersion version,
                         HttpMethod method,
                         const std::string& uri,
                         const HeaderFieldList& headers,
                         bool secure,
                         HugeBuffer&& content)
    : HttpRequest(version, fmt::format("{}", method), uri, headers, secure, std::move(content)) {
}

HttpRequest::HttpRequest(HttpVersion version,
                         const std::string& method,
                         const std::string& uri,
                         const HeaderFieldList& headers,
                         bool secure,
                         HugeBuffer&& content)
    : HttpRequestInfo(version, method, uri, 0, headers),
      remoteAddress_(),
      localAddress_(),
      bytesReceived_(0),
      host_(headers.get("Host")),
      secure_(secure),
      expect100Continue_(false),
      content_(std::move(content)),
      onContentReady_(),
      onContentAvailable_(),
      username_() {
}

void HttpRequest::setRemoteAddress(const std::optional<InetAddress>& inet) {
  remoteAddress_ = inet;
}

const std::optional<InetAddress>& HttpRequest::remoteAddress() const {
  return remoteAddress_;
}

void HttpRequest::setLocalAddress(const std::optional<InetAddress>& inet) {
  localAddress_ = inet;
}

const std::optional<InetAddress>& HttpRequest::localAddress() const {
  return localAddress_;
}

void HttpRequest::recycle() {
  TRACE("{} recycle", (void*)this);

  HttpRequestInfo::reset();

  remoteAddress_.reset();
  localAddress_.reset();
  bytesReceived_ = 0;
  secure_ = false;
  expect100Continue_ = false;
  host_.clear();
  content_.clear();
  username_.clear();
}

void HttpRequest::setHost(const std::string& value) {
  host_ = value;
}

void HttpRequest::discardContent(std::function<void()> onReady) {
  onContentAvailable_ = nullptr;
  onContentReady_ = std::move(onReady);
}

void HttpRequest::consumeContent(std::function<void()> onReady) {
  onContentAvailable_ = [this](const BufferRef& chunk) {
    content_.write(chunk);
  };

  onContentReady_ = std::move(onReady);
}

void HttpRequest::fillContent(const BufferRef& chunk) {
  TRACE("fillContent {} bytes: '{}'", chunk.size(), chunk);
  setContentLength(contentLength() + chunk.size());

  if (onContentAvailable_) {
    onContentAvailable_(chunk);
  }
}

void HttpRequest::ready() {
  if (onContentReady_)
    onContentReady_();
}

HugeBuffer& HttpRequest::getContent() {
  return content_;
}

const HugeBuffer& HttpRequest::getContent() const {
  return content_;
}

}  // namespace http
}  // namespace xzero
