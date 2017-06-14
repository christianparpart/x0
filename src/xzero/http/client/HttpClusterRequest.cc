// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/MonotonicClock.h>
#include <xzero/TokenShaper.h>
#include <xzero/JsonWriter.h>
#include <xzero/logging.h>
#include <assert.h>

namespace xzero {
namespace http {
namespace client {

#if !defined(NDEBUG)
# define DEBUG(msg...) logDebug("http.client.HttpClusterRequest", msg)
# define TRACE(msg...) logTrace("http.client.HttpClusterRequest", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

HttpClusterRequest::HttpClusterRequest(const HttpRequestInfo& _requestInfo,
                                       const BufferRef& _requestBody,
                                       std::unique_ptr<HttpListener> _responseListener,
                                       Executor* _executor,
                                       size_t responseBodyBufferSize,
                                       const std::string& proxyId)
    : ctime(MonotonicClock::now()),
      client(_executor, responseBodyBufferSize),
      executor(_executor),
      bucket(nullptr),
      backend(nullptr),
      tryCount(0),
      tokens(0),
      proxyVersion_(_requestInfo.version()),
      proxyId_(proxyId),
      viaText_(),
      responseListener(std::move(_responseListener)) {
  TRACE("ctor: executor: $0", executor);
  client.setRequest(_requestInfo, _requestBody);
}

HttpClusterRequest::~HttpClusterRequest() {
}

void HttpClusterRequest::onMessageBegin(HttpVersion version, HttpStatus code,
                                        const BufferRef& text) {
  responseListener->onMessageBegin(version, code, text);
}

void HttpClusterRequest::onMessageHeader(const BufferRef& name,
                                         const BufferRef& value) {
  if (name == "Via") {
    if (!viaText_.empty()) {
      viaText_ += ' ';
    }
    viaText_ += value;
  } else {
    responseListener->onMessageHeader(name, value);
  }
}

void HttpClusterRequest::onMessageHeaderEnd() {
  // RFC 7230, section 5.7.1: makes it clear, that we put ourselfs into the
  // front of the Via-list.

  if (!proxyId_.empty()) {
    Buffer buf;
    buf.reserve(proxyId_.size() + viaText_.size() + 8);
    buf.push_back(StringUtil::toString(proxyVersion_));
    buf.push_back(' ');
    buf.push_back(proxyId_);
    buf.push_back(viaText_);
    responseListener->onMessageHeader("Via", buf);
  } else if (!viaText_.empty()) {
    responseListener->onMessageHeader("Via", viaText_);
  }

  responseListener->onMessageHeaderEnd();
}

void HttpClusterRequest::onMessageContent(const BufferRef& chunk) {
  responseListener->onMessageContent(chunk);
}

void HttpClusterRequest::onMessageContent(FileView&& chunk) {
  responseListener->onMessageContent(std::move(chunk));
}

void HttpClusterRequest::onMessageEnd() {
  TRACE("onMessageEnd!");

  // FYI: timed out requests do not have tokens, backend and bucket
  if (tokens) {
    assert(bucket != nullptr);
    assert(backend != nullptr);
    bucket->put(tokens);
    backend->release();
  }
  responseListener->onMessageEnd();
}

void HttpClusterRequest::onProtocolError(HttpStatus code,
                                         const std::string& message) {
  responseListener->onProtocolError(code, message);
}

} // namespace client
} // namespace http

template <>
JsonWriter& JsonWriter::value(
    TokenShaper<http::client::HttpClusterRequest> const& value) {
  value.writeJSON(*this);
  return *this;
}

template <>
JsonWriter& JsonWriter::value(
    typename TokenShaper<http::client::HttpClusterRequest>::Node const& value) {
  value.writeJSON(*this);
  return *this;
}

} // namespace xzero
