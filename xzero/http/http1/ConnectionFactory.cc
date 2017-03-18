// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero/http/http1/Connection.h>
#include <xzero/net/Connector.h>
#include <xzero/net/EndPoint.h>

namespace xzero {
namespace http {
namespace http1 {

ConnectionFactory::ConnectionFactory()
    : ConnectionFactory(8 * 1024,
                        8 * 1024,
                        4096,
                        4 * 1024 * 1024,
                        100,
                        8_seconds,
                        false,
                        false) {
}

ConnectionFactory::ConnectionFactory(
    size_t requestHeaderBufferSize,
    size_t requestBodyBufferSize,
    size_t maxRequestUriLength,
    size_t maxRequestBodyLength,
    size_t maxRequestCount,
    Duration maxKeepAlive,
    bool corkStream,
    bool tcpNoDelay)
    : HttpConnectionFactory("http/1.1", maxRequestUriLength,
                            maxRequestBodyLength),
      requestHeaderBufferSize_(requestHeaderBufferSize),
      requestBodyBufferSize_(requestBodyBufferSize),
      maxRequestCount_(maxRequestCount),
      maxKeepAlive_(maxKeepAlive),
      corkStream_(corkStream),
      tcpNoDelay_(tcpNoDelay) {
}

ConnectionFactory::~ConnectionFactory() {
}

::xzero::Connection* ConnectionFactory::create(Connector* connector,
                                               EndPoint* endpoint) {
  if (tcpNoDelay_) {
    endpoint->setTcpNoDelay(true);
  }

  const size_t inputBufferSize =
      requestHeaderBufferSize_ + requestBodyBufferSize_;

  return endpoint->setConnection<http1::Connection>(endpoint,
                                                    connector->executor(),
                                                    handler(),
                                                    dateGenerator(),
                                                    outputCompressor(),
                                                    maxRequestUriLength(),
                                                    maxRequestBodyLength(),
                                                    maxRequestCount(),
                                                    maxKeepAlive(),
                                                    inputBufferSize,
                                                    corkStream());
}

}  // namespace http1
}  // namespace http
}  // namespace xzero
