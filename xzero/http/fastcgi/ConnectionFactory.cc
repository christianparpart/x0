// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/fastcgi/ConnectionFactory.h>
#include <xzero/http/fastcgi/Connection.h>
#include <xzero/net/Connector.h>
#include <xzero/net/EndPoint.h>
#include <xzero/WallClock.h>

namespace xzero {
namespace http {
namespace fastcgi {

ConnectionFactory::ConnectionFactory()
    : ConnectionFactory(4096,
                        4 * 1024 * 1024,
                        8_seconds) {
}

ConnectionFactory::ConnectionFactory(
    size_t maxRequestUriLength,
    size_t maxRequestBodyLength,
    Duration maxKeepAlive)
    : HttpConnectionFactory("fastcgi",
                            maxRequestUriLength,
                            maxRequestBodyLength),
      maxKeepAlive_(maxKeepAlive) {
}

ConnectionFactory::~ConnectionFactory() {
}

xzero::Connection* ConnectionFactory::create(Connector* connector,
                                             EndPoint* endpoint) {
  return endpoint->setConnection<Connection>(endpoint,
                                             connector->executor(),
                                             handler(),
                                             dateGenerator(),
                                             outputCompressor(),
                                             maxRequestUriLength(),
                                             maxRequestBodyLength(),
                                             maxKeepAlive());
}

} // namespace fastcgi
} // namespace http
} // namespace xzero
