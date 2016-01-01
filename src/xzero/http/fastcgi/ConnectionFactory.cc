// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
  setInputBufferSize(16 * 1024);
}

ConnectionFactory::~ConnectionFactory() {
}

xzero::Connection* ConnectionFactory::create(
    Connector* connector,
    EndPoint* endpoint) {
  return configure(endpoint->setConnection<Connection>(endpoint,
                                                       connector->executor(),
                                                       handler(),
                                                       dateGenerator(),
                                                       outputCompressor(),
                                                       maxRequestUriLength(),
                                                       maxRequestBodyLength(),
                                                       maxKeepAlive()),
                   connector);
}

} // namespace fastcgi
} // namespace http
} // namespace xzero
