#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/MonotonicClock.h>
#include <xzero/logging.h>
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

void HttpClusterRequest::onMessageBegin(HttpVersion version, HttpStatus code,
                                        const BufferRef& text) {
  responseListener->onMessageBegin(version, code, text);
}

void HttpClusterRequest::onMessageHeader(const BufferRef& name,
                                         const BufferRef& value) {
  responseListener->onMessageHeader(name, value);
}

void HttpClusterRequest::onMessageHeaderEnd() {
  logNotice("HttpClusterRequest", "onMessageHeaderEnd");
  responseListener->onMessageHeaderEnd();
}

void HttpClusterRequest::onMessageContent(const BufferRef& chunk) {
  responseListener->onMessageContent(chunk);
}

void HttpClusterRequest::onMessageEnd() {
  logNotice("HttpClusterRequest", "onMessageEnd ($0 $1)", bucket != nullptr, tokens);
  if (bucket != nullptr && tokens != 0) {
    logNotice("HttpClusterRequest", "put $0 tokens into bucket $1",
        tokens, bucket->name());
    bucket->put(tokens);
  }

  responseListener->onMessageEnd();
}

void HttpClusterRequest::onProtocolError(HttpStatus code,
                                         const std::string& message) {
  logNotice("HttpClusterRequest", "onProtocolError");
  responseListener->onProtocolError(code, message);
}

} // namespace http
} // namespace client
} // namespace xzero
