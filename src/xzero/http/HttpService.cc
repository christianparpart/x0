// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpService.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero/http/fastcgi/ConnectionFactory.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/RuntimeError.h>
#include <xzero/WallClock.h>
#include <xzero/logging.h>
#include <algorithm>
#include <stdexcept>

namespace xzero {
namespace http {

template<typename... Args> constexpr void TRACE(const char* msg, Args... args) {
#ifndef NDEBUG
  ::xzero::logTrace(std::string("http.HttpService: ") + msg, args...);
#endif
}

HttpService::HttpService()
    : HttpService(getDefaultProtocol()) {
}

HttpService::Protocol HttpService::getDefaultProtocol() {
  const char* env = getenv("HTTP_TRANSPORT");
  if (env == nullptr || !*env)
    return HttpService::HTTP1;

  if (strcmp(env, "fastcgi") == 0)
    return HttpService::FCGI;

  if (strcmp(env, "fcgi") == 0)
    return HttpService::FCGI;

  if (strcmp(env, "http") == 0)
    return HttpService::HTTP1;

  if (strcmp(env, "http1") == 0)
    return HttpService::HTTP1;

  throw std::invalid_argument{"HTTP_TRANSPORT"};
  // RuntimeError{"Invalid value for environment variable HTTP_TRANSPORT: \"%s\".",
  //       env};
}

HttpService::HttpService(Protocol protocol)
    : protocol_{protocol},
      inetConnector_{nullptr},
      handlers_{} {
}

HttpService::HttpService(Executor* executor, int port)
    : HttpService(executor, port, IPAddress{"0.0.0.0"}) {
}

HttpService::HttpService(Executor* executor, int port, const IPAddress& bind)
    : protocol_{getDefaultProtocol()},
      inetConnector_{nullptr},
      handlers_{} {
  configureTcp(executor, executor, 60_seconds, 20_seconds, 0_seconds,
      bind, port, 128);
}

HttpService::~HttpService() {
}

TcpConnector* HttpService::configureTcp(Executor* executor,
                                        Executor* clientExecutor,
                                        Duration readTimeout,
                                        Duration writeTimeout,
                                        Duration tcpFinTimeout,
                                        const IPAddress& ipaddress,
                                        int port, int backlog) {
  if (inetConnector_ != nullptr)
    logFatal("Multiple inet connectors not yet supported.");

  inetConnector_ = std::make_unique<TcpConnector>(
      "http", executor,
      [clientExecutor]() { return clientExecutor; },
      readTimeout, writeTimeout,
      tcpFinTimeout, ipaddress, port, backlog, true, false);

  attachProtocol(inetConnector_.get());

  return inetConnector_.get();
}

void HttpService::attachProtocol(TcpConnector* connector) {
  switch (protocol_) {
    case HTTP1:
      attachHttp1(connector);
      break;
    case FCGI:
      attachFCGI(connector);
      break;
  }
}

void HttpService::attachHttp1(TcpConnector* connector) {
  // TODO: make them configurable via ctor?
  constexpr size_t requestHeaderBufferSize = 8 * 1024;
  constexpr size_t requestBodyBufferSize = 8 * 1024;
  constexpr size_t maxRequestUriLength = 1024;
  constexpr size_t maxRequestBodyLength = 64 * 1024 * 1024;
  constexpr size_t maxRequestCount = 100;
  constexpr Duration maxKeepAlive = 8_seconds;
  constexpr bool corkStream = false;
  constexpr bool tcpNoDelay = false;

  auto http1 = std::make_unique<xzero::http::http1::ConnectionFactory>(
      requestHeaderBufferSize,
      requestBodyBufferSize,
      maxRequestUriLength,
      maxRequestBodyLength,
      maxRequestCount,
      maxKeepAlive,
      corkStream,
      tcpNoDelay);

  http1->setHandlerFactory(std::bind(&HttpService::createHandler, this,
                                     std::placeholders::_1,
                                     std::placeholders::_2));

  connector->addConnectionFactory(
      http1->protocolName(),
      std::bind(&HttpConnectionFactory::create, http1.get(),
                std::placeholders::_1,
                std::placeholders::_2));

  httpFactories_.emplace_back(std::move(http1));
}

void HttpService::attachFCGI(TcpConnector* connector) {
  size_t maxRequestUriLength = 1024;
  size_t maxRequestBodyLength = 64 * 1024 * 1024;
  Duration maxKeepAlive = 8_seconds;

  auto fcgi = std::make_unique<http::fastcgi::ConnectionFactory>(
      maxRequestUriLength, maxRequestBodyLength, maxKeepAlive);

  fcgi->setHandlerFactory(std::bind(&HttpService::createHandler, this,
                                    std::placeholders::_1,
                                    std::placeholders::_2));

  connector->addConnectionFactory(
    fcgi->protocolName(),
    std::bind(&HttpConnectionFactory::create, fcgi.get(),
              std::placeholders::_1,
              std::placeholders::_2));

  httpFactories_.emplace_back(std::move(fcgi));
}

void HttpService::addHandler(Handler handler) {
  handlers_.push_back(handler);
}

void HttpService::start() {
  if (inetConnector_)
    inetConnector_->start();
}

void HttpService::stop() {
  if (inetConnector_)
    inetConnector_->stop();
}

class HttpServiceHandler {
 public:
  HttpServiceHandler(HttpService& svc, HttpRequest* in, HttpResponse* out)
    : service(svc), request(in), response(out)
  {}

  void handleRequest() {
    if (request->expect100Continue())
      response->send100Continue(nullptr);

    request->consumeContent(std::bind(&HttpServiceHandler::onAllDataRead, this));
  }

  void onAllDataRead() {
    for (auto& handler: service.handlers())
      if (handler(request, response))
        return;

    response->setStatus(HttpStatus::NotFound);
    response->completed();
  }

  const HttpService& service;
  HttpRequest* request;
  HttpResponse* response;
};

std::function<void()> HttpService::createHandler(HttpRequest* request,
                                                 HttpResponse* response) {
  return std::bind(&HttpService::handleRequest, this, request, response);
}

void HttpService::handleRequest(HttpRequest* request, HttpResponse* response) {
  if (request->expect100Continue())
    response->send100Continue(nullptr);

  request->consumeContent(
      std::bind(&HttpService::onAllDataRead, this, request, response));
}

void HttpService::onAllDataRead(HttpRequest* request, HttpResponse* response) {
  for (Handler& handler: handlers_) {
    if (handler(request, response)) {
      return;
    }
  }

  response->setStatus(HttpStatus::NotFound);
  response->completed();
}

// {{{ BuiltinAssetHandler
HttpService::BuiltinAssetHandler::BuiltinAssetHandler()
    : assets_() {
}

void HttpService::BuiltinAssetHandler::addAsset(const std::string& path,
                                                const std::string& mimetype,
                                                const Buffer&& data) {
  assets_[path] = { mimetype, UnixTime(), std::move(data) };
}

bool HttpService::BuiltinAssetHandler::handleRequest(HttpRequest* request,
                                                     HttpResponse* response) {
  static const char* timeFormat = "%a, %d %b %Y %H:%M:%S GMT";

  auto i = assets_.find(request->path());
  if (i == assets_.end())
    return false;

  // XXX ignores client cache for now

  response->setStatus(HttpStatus::Ok);
  response->setContentLength(i->second.data.size());
  response->addHeader("Content-Type", i->second.mimetype);
  response->addHeader("Last-Modified", i->second.mtime.format(timeFormat));
  response->write(i->second.data.ref());
  response->completed();

  return true;
}
// }}}

} // namespace http
} // namespace xzero
