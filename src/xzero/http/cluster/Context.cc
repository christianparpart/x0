// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/cluster/Context.h>
#include <xzero/http/cluster/Backend.h>
#include <xzero/MonotonicClock.h>
#include <xzero/TokenShaper.h>
#include <xzero/JsonWriter.h>
#include <xzero/logging.h>
#include <assert.h>

namespace xzero::http::cluster {

#if !defined(NDEBUG)
# define DEBUG(msg...) logDebug("http.cluster.Context: " msg)
# define TRACE(msg...) logTrace("http.cluster.Context: " msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

Context::Context(const HttpRequest& _request,
                 std::unique_ptr<HttpListener> _responseListener,
                 Executor* _executor,
                 size_t responseBodyBufferSize,
                 const std::string& proxyId)
    : ctime{MonotonicClock::now()},
      executor{_executor},
      bucket{nullptr},
      backend{nullptr},
      client{nullptr},
      tryCount{0},
      tokens{0},
      request{_request},
      proxyVersion_{request.version()},
      proxyId_{proxyId},
      viaText_{},
      responseListener{std::move(_responseListener)} {
  TRACE("ctor: executor: {}", executor);
}

Context::~Context() {
}

void Context::onMessageBegin(HttpVersion version, HttpStatus code,
                             const BufferRef& text) {
  responseListener->onMessageBegin(version, code, text);
}

void Context::onMessageHeader(const BufferRef& name,
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

void Context::onMessageHeaderEnd() {
  // RFC 7230, section 5.7.1: makes it clear, that we put ourselfs into the
  // front of the Via-list.

  if (!proxyId_.empty()) {
    Buffer buf;
    buf.reserve(proxyId_.size() + viaText_.size() + 8);
    buf.push_back(to_string(proxyVersion_));
    buf.push_back(' ');
    buf.push_back(proxyId_);
    buf.push_back(viaText_);
    responseListener->onMessageHeader("Via", buf);
  } else if (!viaText_.empty()) {
    responseListener->onMessageHeader("Via", viaText_);
  }

  responseListener->onMessageHeaderEnd();
}

void Context::onMessageContent(const BufferRef& chunk) {
  responseListener->onMessageContent(chunk);
}

void Context::onMessageContent(FileView&& chunk) {
  responseListener->onMessageContent(std::move(chunk));
}

void Context::onMessageEnd() {
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

void Context::onError(std::error_code ec) {
  responseListener->onError(ec);
}

} // namespace xzero::http::cluster

namespace xzero {
  template <>
  JsonWriter& JsonWriter::value(
      TokenShaper<http::cluster::Context> const& value) {
    value.writeJSON(*this);
    return *this;
  }

  template <>
  JsonWriter& JsonWriter::value(
      typename TokenShaper<http::cluster::Context>::Node const& value) {
    value.writeJSON(*this);
    return *this;
  }
} // namespace xzero
