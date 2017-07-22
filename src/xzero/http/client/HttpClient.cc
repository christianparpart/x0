// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClient.h>
#include <xzero/http/client/HttpTransport.h>
#include <xzero/http/client/Http1Connection.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/net/DnsClient.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/InetUtil.h>
#include <xzero/io/FileView.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("HttpClient", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

namespace xzero::http::client {

template<typename T> static bool isConnectionHeader(const T& name) { // {{{
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
} // }}}

static void removeConnectionHeaders(HeaderFieldList& headers) { // {{{
  // filter out disruptive connection-level headers
  headers.remove("Connection");
  headers.remove("Content-Length");
  headers.remove("Expect");
  headers.remove("Trailer");
  headers.remove("Transfer-Encoding");
  headers.remove("Upgrade");
} // }}}

HttpClient::HttpClient(Executor* executor,
                       RefPtr<EndPoint> upstream)
    : executor_(executor),
      endpoint_(upstream) {
  setupConnection();
}

HttpClient::HttpClient(Executor* executor, const InetAddress& upstream)
    : HttpClient(executor,
                 upstream,
                 5_seconds,
                 5_seconds,
                 5_seconds) {
}

HttpClient::HttpClient(Executor* executor,
                       const InetAddress& upstream,
                       Duration connectTimeout,
                       Duration readTimeout,
                       Duration writeTimeout)
    : executor_(executor),
      endpoint_() {
  Future<int> f = InetUtil::connect(upstream, connectTimeout, executor);
  f.onSuccess(std::bind(&HttpClient::onConnected, this,
                        std::placeholders::_1,
                        upstream.family(),
                        readTimeout,
                        writeTimeout));
  f.onFailure([upstream](std::error_code ec) {
      logError("HttpClient", "Failed to connect to $0. $1", upstream, ec.message());
  });
}

HttpClient::HttpClient(HttpClient&& other)
    : executor_(other.executor_),
      endpoint_(std::move(other.endpoint_)) {
}

void HttpClient::onConnected(int fd, int family, Duration rt, Duration wt) {
  endpoint_ = RefPtr<EndPoint>(new InetEndPoint(fd, family, rt, wt, executor_));
  setupConnection();
}

void HttpClient::setupConnection() {
  endpoint_->setConnection<Http1Connection>(this, endpoint_.get(), executor_);
}

HttpTransport* HttpClient::getChannel() {
  return (HttpTransport*) (endpoint_->connection());
}

void HttpClient::send(const Request& request,
                      HttpListener* responseListener) {
  if (HttpTransport* channel = getChannel(); channel != nullptr) {
    channel->setListener(responseListener);
    channel->send(request, nullptr);
    channel->send(request.getContent().getBuffer(), nullptr);
    channel->completed();
  }
}

void HttpClient::send(const Request& request,
            std::function<void(Response&&)> onSuccess,
            std::function<void(std::error_code)> onFailure) {
}

// ----------------------------------------------------------------------------
HttpClient::ResponseBuilder::ResponseBuilder(std::function<void(Response&&)> s,
                                             std::function<void(std::error_code)> f)
    : success_(s),
      failure_(f),
      response_() {
}

void HttpClient::ResponseBuilder::onMessageBegin(HttpVersion version,
                                                 HttpStatus code,
                                                 const BufferRef& text) {
  TRACE("onMessageBegin($0, $1, $2)", version, (int)code, text);

  response_.setVersion(version);
  response_.setStatus(code);
  response_.setReason(text.str());
}

void HttpClient::ResponseBuilder::onMessageHeader(const BufferRef& name,
                                                  const BufferRef& value) {
  TRACE("onMessageHeader($0, $1)", name, value);

  response_.headers().push_back(name.str(), value.str());
}

void HttpClient::ResponseBuilder::onMessageHeaderEnd() {
  TRACE("onMessageHeaderEnd()");
}

void HttpClient::ResponseBuilder::onMessageContent(const BufferRef& chunk) {
  TRACE("onMessageContent(BufferRef) $0 bytes", chunk.size());
  response_.content().write(chunk);
}

void HttpClient::ResponseBuilder::onMessageContent(FileView&& chunk) {
  TRACE("onMessageContent(FileView) $0 bytes", chunk.size());
  response_.content().write(std::move(chunk));
}

void HttpClient::ResponseBuilder::onMessageEnd() {
  TRACE("onMessageEnd()");
  response_.setContentLength(response_.content().size());
  success_(std::move(response_));
}

void HttpClient::ResponseBuilder::onError(std::error_code ec) {
  logError("ResponseBuilder", "Error. $0; $1", ec.message());
  failure_(ec);
}

} // namespace xzero::http::client
