// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/proxy/HttpReverseProxy.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>

namespace xzero::http::client {

HttpReverseProxy::HttpReverseProxy(Executor* executor,
                                   const InetAddress& upstream)
    : HttpReverseProxy{executor, upstream, 1, 10_seconds, ErrorPageHandler{}} {
}

HttpReverseProxy::HttpReverseProxy(Executor* executor,
                                   const InetAddress& upstream,
                                   size_t maxPoolSize,
                                   Duration keepAliveTimeout,
                                   ErrorPageHandler errorPageHandler)
    : HttpReverseProxy{
          executor,
          std::bind(&HttpReverseProxy::doConnect, this, upstream),
          maxPoolSize,
          keepAliveTimeout,
          errorPageHandler} {
}

HttpReverseProxy::HttpReverseProxy(Executor* executor,
                                   EndPointFactory endpointFactory,
                                   size_t maxPoolSize,
                                   Duration keepAliveTimeout,
                                   ErrorPageHandler errorPageHandler)
    : executor_{executor},
      endpointFactory_{endpointFactory},
      maxPoolSize_{maxPoolSize},
      keepAliveTimeout_{keepAliveTimeout},
      errorPageHandler_{errorPageHandler} {
}

HttpReverseProxy::~HttpReverseProxy() {
}

TcpEndPoint* HttpReverseProxy::doConnect(const InetAddress& inetAddress) {
  return nullptr; // TODO
}

} // namespace xzero::http::client
