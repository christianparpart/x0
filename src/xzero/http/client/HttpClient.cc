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
#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/DnsClient.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/io/FileView.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("HttpClient", msg)
#else
#warning "No NDEBUG set"
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

HttpClient::HttpClient(Executor* executor,
                       const InetAddress& upstream)
    : HttpClient(executor,
                 upstream,
                 10_seconds,   // connectTimeout
                 5_minutes,    // readTimeout
                 10_seconds,   // writeTimeout
                 60_seconds) { // keepAlive
}

HttpClient::HttpClient(Executor* executor,
                       const InetAddress& upstream,
                       Duration connectTimeout,
                       Duration readTimeout,
                       Duration writeTimeout,
                       Duration keepAlive)
    : executor_(executor),
      createEndPoint_(std::bind(&HttpClient::createTcp, this,
            upstream,
            connectTimeout,
            readTimeout,
            writeTimeout)),
      keepAlive_(keepAlive),
      endpoint_(),
      pendingTasks_() {
}

HttpClient::HttpClient(Executor* executor,
                       RefPtr<TcpEndPoint> upstream,
                       Duration keepAlive)
    : executor_(executor),
      createEndPoint_(),
      keepAlive_(keepAlive),
      endpoint_(upstream),
      pendingTasks_() {
}

HttpClient::HttpClient(Executor* executor,
                       CreateEndPoint endpointCreator,
                       Duration keepAlive)
    : executor_(executor),
      createEndPoint_(endpointCreator),
      keepAlive_(keepAlive),
      endpoint_(),
      pendingTasks_() {
}

HttpClient::HttpClient(HttpClient&& other)
    : executor_(other.executor_),
      createEndPoint_(std::move(other.createEndPoint_)),
      keepAlive_(std::move(other.keepAlive_)),
      endpoint_(std::move(other.endpoint_)),
      pendingTasks_(std::move(other.pendingTasks_)) {
}

bool HttpClient::isClosed() const {
  return !endpoint_;
}

Future<RefPtr<TcpEndPoint>> HttpClient::createTcp(InetAddress addr,
                                                  Duration connectTimeout,
                                                  Duration readTimeout,
                                                  Duration writeTimeout) {
  Promise<RefPtr<TcpEndPoint>> promise;

  Future<int> f = TcpUtil::connect(addr, connectTimeout, executor_);

  f.onFailure(promise);
  f.onSuccess([promise, addr, readTimeout, writeTimeout, this](int fd) {
    promise.success(RefPtr<TcpEndPoint>(new TcpEndPoint(fd,
                                                        addr.family(),
                                                        readTimeout,
                                                        writeTimeout,
                                                        executor_,
                                                        nullptr)));
  });

  return promise.future();
}

Future<HttpClient::Response> HttpClient::send(const std::string& method,
                                              const Uri& url,
                                              const HeaderFieldList& headers) {
  return send(Request{HttpVersion::VERSION_1_1,
                      method,
                      url.toString(),
                      headers,
                      false,
                      HugeBuffer()});
}

Future<HttpClient::Response> HttpClient::send(const Request& request) {
  Promise<Response> promise;
  send(request,
       [promise](const Response& response) {
         TRACE("send: response received");
         promise.success(std::move(response));
       },
       [promise](std::error_code ec) {
         TRACE("send: failure received");
         promise.failure(ec);
       });
  return promise.future();
}

void HttpClient::send(const Request& request,
            std::function<void(const Response&)> onSuccess,
            std::function<void(std::error_code)> onFailure) {
  pendingTasks_.emplace_back(Task{request, new ResponseBuilder(onSuccess, onFailure), true});
  tryConsumeTask();
}

void HttpClient::send(const Request& request,
                      HttpListener* responseListener) {
  pendingTasks_.emplace_back(Task{request, responseListener, false});
  tryConsumeTask();
}

bool HttpClient::tryConsumeTask() {
  if (pendingTasks_.empty())
    return false;

  auto f = createEndPoint_();
  f.onSuccess([this](RefPtr<TcpEndPoint> ep) {
    TRACE("endpoint created");
    endpoint_ = ep;

    Task task {std::move(pendingTasks_.front())};
    pendingTasks_.pop_front();

    HttpTransport* channel = endpoint_->setConnection<Http1Connection>(
        task.listener, endpoint_.get(), executor_);

    channel->setListener(task.listener);
    channel->send(task.request, nullptr);
    channel->send(task.request.getContent().getBuffer(), nullptr);
    channel->completed();
  });
  f.onFailure([this](std::error_code ec) {
    logError("HttpClient", "Failed to connect. $0", ec.message());
  });

  return true;
}

void HttpClient::setupConnection() {
  TRACE("setting up connection");
  // XXX I am not happy. looks like I'll need a channel abstraction in the client-code, too
  const Task& task = pendingTasks_.front();

  HttpTransport* transport = endpoint_->setConnection<Http1Connection>(
      task.listener, endpoint_.get(), executor_);

  transport->send(task.request, nullptr);
  //TODO: transport->send(task.request.getContent(), nullptr);
}

HttpTransport* HttpClient::getChannel() {
  return (HttpTransport*) (endpoint_->connection());
}

// ----------------------------------------------------------------------------
HttpClient::ResponseBuilder::ResponseBuilder(std::function<void(Response&&)> s,
                                             std::function<void(std::error_code)> f)
    : success_(s),
      failure_(f),
      response_() {
  TRACE("ResponseBuilder.ctor");
}

void HttpClient::ResponseBuilder::onMessageBegin(HttpVersion version,
                                                 HttpStatus code,
                                                 const BufferRef& text) {
  TRACE("ResponseBuilder.onMessageBegin($0, $1, $2)", version, (int)code, text);

  response_.setVersion(version);
  response_.setStatus(code);
  response_.setReason(text.str());
}

void HttpClient::ResponseBuilder::onMessageHeader(const BufferRef& name,
                                                  const BufferRef& value) {
  TRACE("ResponseBuilder.onMessageHeader($0, $1)", name, value);

  response_.headers().push_back(name.str(), value.str());
}

void HttpClient::ResponseBuilder::onMessageHeaderEnd() {
  TRACE("ResponseBuilder.onMessageHeaderEnd()");
}

void HttpClient::ResponseBuilder::onMessageContent(const BufferRef& chunk) {
  TRACE("ResponseBuilder.onMessageContent(BufferRef) $0 bytes", chunk.size());
  response_.content().write(chunk);
}

void HttpClient::ResponseBuilder::onMessageContent(FileView&& chunk) {
  TRACE("ResponseBuilder.onMessageContent(FileView) $0 bytes", chunk.size());
  response_.content().write(std::move(chunk));
}

void HttpClient::ResponseBuilder::onMessageEnd() {
  TRACE("ResponseBuilder.onMessageEnd()");
  response_.setContentLength(response_.content().size());
  success_(std::move(response_));
  delete this;
}

void HttpClient::ResponseBuilder::onError(std::error_code ec) {
  logError("ResponseBuilder", "Error. $0; $1", ec.message());
  failure_(ec);
  delete this;
}

} // namespace xzero::http::client
