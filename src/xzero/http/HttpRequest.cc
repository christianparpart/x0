// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpRequest.h>
#include <xzero/io/BufferInputStream.h>
#include <xzero/io/FileInputStream.h>
#include <xzero/io/InputStream.h>
#include <xzero/io/FileUtil.h>
#include <xzero/logging.h>

#include <unistd.h>

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
    : HttpRequest("", "", HttpVersion::UNKNOWN, false, {}, Buffer()) {
  // .
}

HttpRequest::HttpRequest(const std::string& method, const std::string& path,
                         HttpVersion version, bool secure,
                         const HeaderFieldList& headers,
                         Buffer&& content)
    : HttpRequestInfo(version, method, path, 0, headers),
      remoteAddress_(),
      localAddress_(),
      bytesReceived_(0),
      host_(headers.get("Host")),
      secure_(secure),
      expect100Continue_(false),
      contentBuffer_(std::move(content)),
      contentFd_(-1),
      onContentReady_(),
      onContentAvailable_(),
      username_() {
  // .
}

void HttpRequest::setRemoteAddress(const Option<InetAddress>& inet) {
  remoteAddress_ = inet;
}

const Option<InetAddress>& HttpRequest::remoteAddress() const {
  return remoteAddress_;
}

void HttpRequest::setLocalAddress(const Option<InetAddress>& inet) {
  localAddress_ = inet;
}

const Option<InetAddress>& HttpRequest::localAddress() const {
  return localAddress_;
}

void HttpRequest::recycle() {
  TRACE("$0 recycle", this);

  HttpRequestInfo::reset();

  remoteAddress_.reset();
  localAddress_.reset();
  bytesReceived_ = 0;
  secure_ = false;
  expect100Continue_ = false;
  host_.clear();
  contentBuffer_.clear();
  contentFd_.close();
  username_.clear();
}

void HttpRequest::setHost(const std::string& value) {
  host_ = value;
}

void HttpRequest::discardContent(std::function<void()> onReady) {
  onContentAvailable_ = nullptr;
  onContentReady_ = onReady;
}

void HttpRequest::consumeContent(std::function<void()> onReady) {
  onContentAvailable_ = [this](const BufferRef& chunk) {
    const size_t maxBufferSize = 1024; // TODO: pass me

    if (contentFd_ < 0 && contentBuffer_.size() + chunk.size() > maxBufferSize) {
      contentFd_ = FileUtil::createTempFile();
    }

    if (contentFd_ < 0) {
      contentBuffer_.push_back(chunk);
      TRACE("$0: consuming $1 bytes of content", this, chunk.size());
    } else {
      size_t offset = 0;

      while (offset < chunk.size()) {
        ssize_t n = ::write(contentFd_,
                            chunk.data() + offset,
                            chunk.size() - offset);

        if (n > 0)
          offset += n;
        else if (n < 0 && errno != EINTR)
          RAISE_ERRNO(errno);
      }
    }
  };

  onContentReady_ = onReady;
}

void HttpRequest::fillContent(const BufferRef& chunk) {
  if (onContentAvailable_)
    onContentAvailable_(chunk);
}

void HttpRequest::ready() {
  if (onContentReady_)
    onContentReady_();
}

std::unique_ptr<InputStream> HttpRequest::getContentStream() {
  return std::unique_ptr<InputStream>(
      !contentBuffer_.empty()
          ? static_cast<InputStream*>(new BufferInputStream(&contentBuffer_))
          : static_cast<InputStream*>(new FileInputStream(contentFd_, false)));
}

BufferRef HttpRequest::getContentBuffer() {
  if (!contentBuffer_.empty())
    return contentBuffer_;

  std::unique_ptr<InputStream> content = getContentStream();
  while (content->read(&contentBuffer_, 4096) > 0)
    ;

  return contentBuffer_;
}

}  // namespace http

template<>
std::string StringUtil::toString(http::HttpRequest* value) {
  return StringUtil::format("HttpRequest[$0]", (void*)value);
}

}  // namespace xzero
