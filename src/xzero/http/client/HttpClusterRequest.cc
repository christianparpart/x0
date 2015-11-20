#include <xzero/http/client/HttpClusterRequest.h>

namespace xzero {
namespace http {
namespace client {

HttpClusterRequest::HttpClusterRequest(const HttpRequestInfo& _requestInfo,
                                       std::unique_ptr<InputStream> _requestBody,
                                       HttpListener* _responseListener,
                                       TokenShaper<HttpClusterRequest>::Node* _bucket,
                                       Executor* executor)
    : requestInfo(_requestInfo),
      requestBody(std::move(_requestBody)),
      responseListener(_responseListener),
      bucket(_bucket),
      backend(nullptr),
      tryCount(0),
      tokens(0) {
}

} // namespace http
} // namespace client
} // namespace xzero
