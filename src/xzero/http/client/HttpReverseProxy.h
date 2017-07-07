// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/HttpStatus.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/net/InetAddress.h>
#include <xzero/Duration.h>
#include <functional>
#include <list>

namespace xzero {
namespace http {

class HttpRequest;
class HttpResponse;

namespace client {

/**
 * Implements a reverse proxy to another internet HTTP server.
 */
class HttpReverseProxy {
 public:
  typedef std::function<void(HttpStatus, HttpRequest*, HttpResponse*)> ErrorPageHandler;
  typedef std::function<EndPoint*()> EndPointFactory;

  explicit HttpReverseProxy(const InetAddress& upstream);

  /**
   * Initializes the HTTP reverse proxy.
   *
   * @param upstream the upstream endpoint to proxy to.
   * @param maxPoolSize maximum number of upstream connections to keep in background.
   * @param keepAliveTimeout maximum time to keep an idle connection alive.
   */
  HttpReverseProxy(
      const InetAddress& upstream,
      size_t maxPoolSize,
      Duration keepAliveTimeout,
      ErrorPageHandler errorPageHandler);

  /**
   * Initializes the HTTP reverse proxy.
   *
   * @param endpointFactory the upstream endpoint to proxy to.
   * @param maxPoolSize maximum number of upstream connections to keep in background.
   * @param keepAliveTimeout maximum time to keep an idle connection alive.
   */
  HttpReverseProxy(
      EndPointFactory endpointFactory,
      size_t maxPoolSize,
      Duration keepAliveTimeout,
      ErrorPageHandler errorPageHandler);

  ~HttpReverseProxy();

  /**
   * Serves given request to this reverse proxy.
   *
   * This call is thread safe.
   *
   */
  void serve(HttpRequest* request, HttpResponse* response);

 private:
  EndPointFactory endpointFactory_;
  size_t maxPoolSize_;
  Duration keepAliveTimeout_;
  ErrorPageHandler errorPageHandler_;

  std::list<HttpClient> endpoints_;
};

} // namespace client
} // namespace http
} // namespace xzero
