// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClient.h>
#include <xzero/http/client/HttpTransport.h>
#include <xzero/http/client/Http1Connection.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/net/DnsClient.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/io/FileView.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("HttpClient", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

namespace xzero {
namespace http {
namespace client {

template<typename T>
static bool isConnectionHeader(const T& name) {
  static const std::vector<T> connectionHeaderFields = {
    "Connection",
    "Content-Length",
    "Close",
    "Keep-Alive",
    "TE",
    "Trailer",
    "Transfer-Encoding",
    "Upgrade",
  };

  for (const auto& test: connectionHeaderFields)
    if (iequals(name, test))
      return true;

  return false;
}

HttpClient::HttpClient(Executor* executor)
    : HttpClient(executor, 32 * 1024 * 1024) {
}

HttpClient::HttpClient(Executor* executor, size_t responseBodyBufferSize)
    : executor_(executor),
      transport_(nullptr),
      requestInfo_(),
      requestBody_(),
      responseInfo_(),
      responseBody_(responseBodyBufferSize),
      promise_() {
}

HttpClient::HttpClient(HttpClient&& other)
    : executor_(other.executor_),
      transport_(other.transport_),
      requestInfo_(std::move(other.requestInfo_)),
      requestBody_(std::move(other.requestBody_)),
      responseInfo_(std::move(other.responseInfo_)),
      responseBody_(std::move(other.responseBody_)),
      promise_(std::move(other.promise_)) {
  transport_->setListener(this);
  other.transport_ = nullptr;
}

HttpClient::~HttpClient() {
}

void HttpClient::setRequest(const HttpRequestInfo& requestInfo,
                            const BufferRef& requestBody) {
  requestInfo_ = requestInfo;
  requestBody_ = requestBody;

  // filter out disruptive connection-level headers
  requestInfo_.headers().remove("Connection");
  requestInfo_.headers().remove("Content-Length");
  requestInfo_.headers().remove("Expect");
  requestInfo_.headers().remove("Trailer");
  requestInfo_.headers().remove("Transfer-Encoding");
  requestInfo_.headers().remove("Upgrade");
}

Future<HttpClient*> HttpClient::sendAsync(InetAddress& addr,
                                          Duration connectTimeout,
                                          Duration readTimeout,
                                          Duration writeTimeout) {

  Future<RefPtr<EndPoint>> f = InetEndPoint::connectAsync(
      addr, connectTimeout, readTimeout, writeTimeout, executor_);

  f.onSuccess([this](RefPtr<EndPoint> ep) {
    TRACE("onConnected: $0 $1", requestInfo_.method(), requestInfo_.path());
    endpoint_ = ep;
    // is not being freed here explicitely, as the endpoint will own that
    transport_ = new Http1Connection(this, ep.get(), executor_);
    transport_->send(requestInfo_, nullptr);
    transport_->send(requestBody_, nullptr);
    transport_->completed();
  });

  f.onFailure([this](const std::error_code& ec) {
    promise_->failure(ec);
  });

  promise_ = std::unique_ptr<Promise<HttpClient*>>(new Promise<HttpClient*>());
  return promise_->future();
}

void HttpClient::send(RefPtr<EndPoint> ep) {
  transport_ = new Http1Connection(this, ep.get(), executor_);
  transport_->send(requestInfo_, nullptr);
  transport_->send(requestBody_, nullptr);
  transport_->completed();
}

const HttpResponseInfo& HttpClient::responseInfo() const noexcept {
  return responseInfo_;
}

bool HttpClient::isResponseBodyBuffered() const noexcept {
  return !responseBody_.isFile();
}

const BufferRef& HttpClient::responseBody() {
  return responseBody_.getBuffer();
}

FileView HttpClient::takeResponseBody() {
  return responseBody_.getFileView();
}

void HttpClient::onMessageBegin(HttpVersion version, HttpStatus code,
                                const BufferRef& text) {
  TRACE("onMessageBegin($0, $1, $2)", version, (int)code, text);

  responseInfo_.setVersion(version);
  responseInfo_.setStatus(code);
  responseInfo_.setReason(text.str());
}

void HttpClient::onMessageHeader(const BufferRef& name, const BufferRef& value) {
  TRACE("onMessageHeader($0, $1)", name, value);

  responseInfo_.headers().push_back(name.str(), value.str());
}

void HttpClient::onMessageHeaderEnd() {
  TRACE("onMessageHeaderEnd()");
}

void HttpClient::onMessageContent(const BufferRef& chunk) {
  TRACE("onMessageContent(BufferRef) $0 bytes", chunk.size());
  responseBody_.write(chunk);
}

void HttpClient::onMessageContent(FileView&& chunk) {
  TRACE("onMessageContent(FileView) $0 bytes", chunk.size());
  responseBody_.write(std::move(chunk));
}

void HttpClient::onMessageEnd() {
  TRACE("onMessageEnd()");
  responseInfo_.setContentLength(responseBody_.size());
  promise_->success(this);
}

void HttpClient::onProtocolError(HttpStatus code, const std::string& message) {
  logError("HttpClient", "Protocol Error. $0; $1", code, message);
  promise_->failure(std::error_code((int) code, HttpStatusCategory::get()));
}

} // namespace client
} // namespace http
} // namespace xzero
