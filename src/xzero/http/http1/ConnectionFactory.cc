// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
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
    : ConnectionFactory(4096,
                        4 * 1024 * 1024,
                        100,
                        Duration::fromSeconds(8),
                        false,
                        false) {
}

ConnectionFactory::ConnectionFactory(
    size_t maxRequestUriLength,
    size_t maxRequestBodyLength,
    size_t maxRequestCount,
    Duration maxKeepAlive,
    bool corkStream,
    bool tcpNoDelay)
    : HttpConnectionFactory("http/1.1", maxRequestUriLength,
                            maxRequestBodyLength),
      maxRequestCount_(maxRequestCount),
      maxKeepAlive_(maxKeepAlive),
      corkStream_(corkStream),
      tcpNoDelay_(tcpNoDelay) {
  setInputBufferSize(16 * 1024);
}

ConnectionFactory::~ConnectionFactory() {
}

::xzero::Connection* ConnectionFactory::create(Connector* connector,
                                                EndPoint* endpoint) {
  return configure(new http1::Connection(endpoint,
                                         connector->executor(),
                                         handler(),
                                         dateGenerator(),
                                         outputCompressor(),
                                         maxRequestUriLength(),
                                         maxRequestBodyLength(),
                                         maxRequestCount(),
                                         maxKeepAlive(),
                                         corkStream()),
                   connector);
}

::xzero::Connection* ConnectionFactory::configure(
    ::xzero::Connection* connection,
    ::xzero::Connector* connector) {

  if (tcpNoDelay_)
    connection->endpoint()->setTcpNoDelay(true);

  return xzero::ConnectionFactory::configure(connection, connector);
}

}  // namespace http1
}  // namespace http
}  // namespace xzero
