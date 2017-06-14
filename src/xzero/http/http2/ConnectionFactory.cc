// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/ConnectionFactory.h>
#include <xzero/http/http2/Connection.h>
#include <xzero/net/EndPoint.h>
#include <xzero/net/Connector.h>

namespace xzero {
namespace http {
namespace http2 {

ConnectionFactory::ConnectionFactory()
    : ConnectionFactory(256, 16 * 1024 * 1024) {
}

ConnectionFactory::ConnectionFactory(
      size_t maxRequestUriLength,
      size_t maxRequestBodyLength)
    : HttpConnectionFactory("http2",
                            maxRequestUriLength,
                            maxRequestBodyLength) {
}

xzero::Connection* ConnectionFactory::create(Connector* connector,
                                             EndPoint* endpoint) {
  return endpoint->setConnection<http2::Connection>(endpoint,
                                                    connector->executor(),
                                                    handler(),
                                                    dateGenerator(),
                                                    outputCompressor(),
                                                    maxRequestUriLength(),
                                                    maxRequestBodyLength());
}

}  // namespace http1
}  // namespace http
}  // namespace xzero
