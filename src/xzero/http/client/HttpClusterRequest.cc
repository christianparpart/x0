#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/MonotonicClock.h>
#include <assert.h>

namespace xzero {
namespace http {
namespace client {

HttpClusterRequest::HttpClusterRequest(const HttpRequestInfo& _requestInfo,
                                       std::unique_ptr<InputStream> _requestBody,
                                       std::unique_ptr<HttpListener> _responseListener,
                                       Executor* _executor)
    : ctime(MonotonicClock::now()),
      requestInfo(_requestInfo),
      requestBody(std::move(_requestBody)),
      responseListener(std::move(_responseListener)),
      executor(_executor),
      bucket(nullptr),
      backend(nullptr),
      tryCount(0),
      tokens(0) {
}

} // namespace http
} // namespace client
} // namespace xzero
