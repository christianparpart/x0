// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpService.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpInputListener.h>
#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero/http/fastcgi/ConnectionFactory.h>
#include <xzero/net/LocalConnector.h>
#include <xzero/net/InetConnector.h>
#include <xzero/net/Server.h>
#include <xzero/WallClock.h>
#include <algorithm>
#include <stdexcept>

namespace xzero {
namespace http {

class HttpService::InputListener : public HttpInputListener { // {{{
 public:
   InputListener(HttpRequest* request, HttpResponse* response,
                       HttpService* service);
  void onContentAvailable() override;
  void onAllDataRead() override;
  void onError(const std::string& errorMessage) override;

 private:
  HttpRequest* request_;
  HttpResponse* response_;
  HttpService* service_;
};

HttpService::InputListener::InputListener(HttpRequest* request,
                                          HttpResponse* response,
                                          HttpService* service)
    : request_(request),
      response_(response),
      service_(service) {
}

void HttpService::InputListener::onContentAvailable() {
  /* request_->input()->fill(); */
}

void HttpService::InputListener::onAllDataRead() {
  service_->onAllDataRead(request_, response_);
  delete this;
}

void HttpService::InputListener::onError(const std::string& errorMessage) {
  delete this;
}
// }}}

HttpService::HttpService()
    : HttpService(getDefaultProtocol()) {
}

HttpService::Protocol HttpService::getDefaultProtocol() {
  const char* env = getenv("HTTP_TRANSPORT");
  if (env == nullptr)
    return HttpService::HTTP1;

  if (strcmp(env, "fastcgi") == 0)
    return HttpService::FCGI;

  if (strcmp(env, "fcgi") == 0)
    return HttpService::FCGI;

  if (strcmp(env, "http") == 0)
    return HttpService::HTTP1;

  if (strcmp(env, "http1") == 0)
    return HttpService::HTTP1;

  RAISE(RuntimeError,
        "Invalid value for environment variable HTTP_TRANSPORT: \"%s\".",
        env);
}

HttpService::HttpService(Protocol protocol)
    : protocol_(protocol),
      server_(new Server()),
      localConnector_(nullptr),
      inetConnector_(nullptr),
      handlers_() {
}

HttpService::~HttpService() {
  delete server_;
}

LocalConnector* HttpService::configureLocal() {
  if (localConnector_ != nullptr)
    throw std::runtime_error("Multiple local connectors not supported.");

  localConnector_ = server_->addConnector<LocalConnector>();

  attachProtocol(localConnector_);

  return localConnector_;
}

InetConnector* HttpService::configureInet(Executor* executor,
                                          Scheduler* scheduler,
                                          Duration readTimeout,
                                          Duration writeTimeout,
                                          Duration tcpFinTimeout,
                                          const IPAddress& ipaddress,
                                          int port, int backlog) {
  if (inetConnector_ != nullptr)
    throw std::runtime_error("Multiple inet connectors not yet supported.");

  inetConnector_ = server_->addConnector<InetConnector>(
      "http", executor, scheduler, readTimeout, writeTimeout,
      tcpFinTimeout, nullptr, ipaddress, port, backlog, true, false);

  attachProtocol(inetConnector_);

  return inetConnector_;
}

void HttpService::attachProtocol(Connector* connector) {
  switch (protocol_) {
    case HTTP1:
      attachHttp1(connector);
      break;
    case FCGI:
      attachFCGI(connector);
      break;
  }
}

void HttpService::attachHttp1(Connector* connector) {
  // TODO: make them configurable via ctor
  size_t maxRequestUriLength = 1024;
  size_t maxRequestBodyLength = 64 * 1024 * 1024;
  size_t maxRequestCount = 100;
  Duration maxKeepAlive = Duration::fromSeconds(8);

  auto http = connector->addConnectionFactory<xzero::http::http1::ConnectionFactory>(
      maxRequestUriLength,
      maxRequestBodyLength,
      maxRequestCount,
      maxKeepAlive);

  http->setHandler(std::bind(&HttpService::handleRequest, this,
                   std::placeholders::_1, std::placeholders::_2));
}

void HttpService::attachFCGI(Connector* connector) {
  size_t maxRequestUriLength = 1024;
  size_t maxRequestBodyLength = 64 * 1024 * 1024;
  Duration maxKeepAlive = Duration::fromSeconds(8);

  auto fcgi = connector->addConnectionFactory<http::fastcgi::ConnectionFactory>(
      maxRequestUriLength, maxRequestBodyLength, maxKeepAlive);

  fcgi->setHandler(std::bind(&HttpService::handleRequest, this,
                   std::placeholders::_1, std::placeholders::_2));
}

void HttpService::addHandler(Handler* handler) {
  handlers_.push_back(handler);
}

void HttpService::removeHandler(Handler* handler) {
  auto i = std::find(handlers_.begin(), handlers_.end(), handler);
  if (i != handlers_.end())
    handlers_.erase(i);
}

void HttpService::start() {
  server_->start();
}

void HttpService::stop() {
  server_->stop();
}

void HttpService::handleRequest(HttpRequest* request, HttpResponse* response) {
  if (request->expect100Continue())
    response->send100Continue(nullptr);

  request->input()->setListener(new InputListener(request, response, this));
}

void HttpService::onAllDataRead(HttpRequest* request, HttpResponse* response) {
  for (Handler* handler: handlers_) {
    if (handler->handleRequest(request, response)) {
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
  response->output()->write(i->second.data.ref());
  response->completed();

  return true;
}
// }}}

} // namespace http
} // namespace xzero
