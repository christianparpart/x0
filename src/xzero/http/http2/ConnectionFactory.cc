#include <xzero/http/http2/ConnectionFactory.h>
#include <xzero/http/http2/Connection.h>
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

xzero::Connection* ConnectionFactory::create(
    Connector* connector,
    EndPoint* endpoint) {
  return configure(new http2::Connection(endpoint,
                                         connector->executor(),
                                         handler(),
                                         dateGenerator(),
                                         outputCompressor(),
                                         maxRequestUriLength(),
                                         maxRequestBodyLength()),
                   connector);
}

}  // namespace http1
}  // namespace http
}  // namespace xzero
