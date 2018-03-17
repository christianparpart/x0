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
#include <xzero/net/SslEndPoint.h>
#include <xzero/net/DnsClient.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/io/FileView.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("HttpClient: " msg)
#else
#warning "No NDEBUG set"
#define TRACE(msg...) do {} while (0)
#endif

namespace xzero::http::client {

// {{{ ResponseBuilder
class HttpClient::ResponseBuilder : public HttpListener {
 public:
  ResponseBuilder(Promise<Response> promise);

  void onMessageBegin(HttpVersion version, HttpStatus code, const BufferRef& text) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onError(std::error_code ec) override;

 private:
  Promise<Response> promise_;
  Response response_;
};

HttpClient::ResponseBuilder::ResponseBuilder(Promise<Response> promise)
    : promise_(promise),
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
  promise_.success(response_);
  delete this;
}

void HttpClient::ResponseBuilder::onError(std::error_code ec) {
  promise_.failure(ec);
  delete this;
}
// }}}

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
      request_(),
      listener_(),
      isListenerOwned_(false) {
}

HttpClient::HttpClient(Executor* executor,
                       std::shared_ptr<TcpEndPoint> upstream,
                       Duration keepAlive)
    : executor_(executor),
      createEndPoint_(),
      keepAlive_(keepAlive),
      endpoint_(upstream),
      request_(),
      listener_(),
      isListenerOwned_(false) {
}

HttpClient::HttpClient(Executor* executor,
                       CreateEndPoint endpointCreator,
                       Duration keepAlive)
    : executor_(executor),
      createEndPoint_(endpointCreator),
      keepAlive_(keepAlive),
      endpoint_(),
      request_(),
      listener_(),
      isListenerOwned_(false) {
}

HttpClient::HttpClient(HttpClient&& other)
    : executor_(other.executor_),
      createEndPoint_(std::move(other.createEndPoint_)),
      keepAlive_(std::move(other.keepAlive_)),
      endpoint_(std::move(other.endpoint_)),
      request_(std::move(other.request_)),
      listener_(other.listener_),
      isListenerOwned_(other.isListenerOwned_) {
  other.isListenerOwned_ = false;
}

Future<std::shared_ptr<TcpEndPoint>> HttpClient::createTcp(
    InetAddress address,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout) {
  TRACE("createTcp: scheme = '$0'", request_.scheme());
  if (request_.scheme() == "https") {
    TRACE("createTcp: https");
    auto createApplicationConnection = [this](const std::string& protocolName,
                                              TcpEndPoint* endpoint) {
      endpoint->setConnection(std::make_unique<Http1Connection>(listener_,
                                                                endpoint,
                                                                executor_));
    };
    Promise<std::shared_ptr<TcpEndPoint>> promise;
    std::string sni = request_.headers().get("Host");
    size_t i = sni.find(':');
    if (i != std::string::npos) {
      sni = sni.substr(0, i);
    }

    Future<std::shared_ptr<SslEndPoint>> f = SslEndPoint::connect(
        address,
        connectTimeout,
        readTimeout,
        writeTimeout,
        executor_,
        sni,
        {"http/1.1"},
        createApplicationConnection);

    f.onSuccess(promise);
    f.onFailure(promise);
    return promise.future();
  } else {
    TRACE("createTcp: http");
    return TcpEndPoint::connect(address,
                                connectTimeout,
                                readTimeout,
                                writeTimeout,
                                executor_);
  }
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

  request_ = request;
  listener_ = new ResponseBuilder(promise);
  isListenerOwned_ = true;

  execute();

  return promise.future();
}

void HttpClient::send(const Request& request,
                      HttpListener* responseListener) {
  request_ = request;
  listener_ = responseListener;
  isListenerOwned_ = false;

  execute();
}

void HttpClient::execute() {
  Future<std::shared_ptr<TcpEndPoint>> f = createEndPoint_();

  f.onSuccess([this](std::shared_ptr<TcpEndPoint> ep) {
    TRACE("endpoint created");
    endpoint_ = ep;

    if (!endpoint_->connection()) {
      TRACE("creating connection: http/1.1");
      endpoint_->setConnection(std::make_unique<Http1Connection>(
            listener_, endpoint_.get(), executor_));
    }

    // dynamic_cast, as we're having most-likely multiple inheritance here
    HttpTransport* channel = dynamic_cast<HttpTransport*>(endpoint_->connection());
    assert(channel != nullptr);

    channel->setListener(listener_);
    channel->send(request_, nullptr);
    channel->send(request_.getContent().getBuffer(), nullptr);
    channel->completed();
  });

  f.onFailure([this](std::error_code ec) {
    TRACE("Failed to connect. $0: $1", ec.category().name(), ec.message());
    listener_->onError(ec);
  });
}

} // namespace xzero::http::client
