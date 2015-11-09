#include <xzero/http/client/HttpClusterRequest.h>

namespace xzero {
namespace http {
namespace client {

HttpClusterRequest::HttpClusterRequest(HttpRequestInfo&& requestInfo,
                                       std::unique_ptr<InputStream> requestBody,
                                       HttpListener* responseListener)
    : requestInfo_(std::move(requestInfo)),
      requestBody_(std::move(requestBody)),
      responseListener_(responseListener),
      tryCount_(0),
      bucket_(nullptr) {
}

HttpClusterRequest::~HttpClusterRequest() {
}

} // namespace http
} // namespace client
} // namespace xzero
