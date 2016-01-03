#include <xzero/http/http2/Stream.h>
#include <xzero/http/HttpChannel.h>
#include <xzero/DataChain.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace http2 {

#define TRACE(msg...) logTrace("http.http2.Stream", msg)

Stream::Stream(Connection* connection, StreamID id,
               const HttpHandler& handler) {
}

} // namespace http2
} // namespace http
} // namespace xzero
