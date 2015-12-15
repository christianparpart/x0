#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/MonotonicClock.h>
#include <xzero/TokenShaper.h>
#include <xzero/JsonWriter.h>
#include <xzero/logging.h>
#include <assert.h>

namespace xzero {
namespace http {
namespace client {

#if !defined(NDEBUG)
# define DEBUG(msg...) logDebug("http.client.HttpClusterRequest", msg)
# define TRACE(msg...) logTrace("http.client.HttpClusterRequest", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

HttpClusterRequest::HttpClusterRequest(const HttpRequestInfo& _requestInfo,
                                       const BufferRef& _requestBody,
                                       std::unique_ptr<HttpListener> _responseListener,
                                       Executor* _executor)
    : ctime(MonotonicClock::now()),
      client(_executor),
      executor(_executor),
      bucket(nullptr),
      backend(nullptr),
      tryCount(0),
      tokens(0),
      responseListener(std::move(_responseListener)) {
  TRACE("ctor: executor: $0", executor);
  client.setRequest(_requestInfo, _requestBody);
}

HttpClusterRequest::~HttpClusterRequest() {
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
  responseListener->onMessageHeaderEnd();
}

void HttpClusterRequest::onMessageContent(const BufferRef& chunk) {
  responseListener->onMessageContent(chunk);
}

void HttpClusterRequest::onMessageContent(FileView&& chunk) {
  responseListener->onMessageContent(std::move(chunk));
}

void HttpClusterRequest::onMessageEnd() {
  TRACE("onMessageEnd!");

  assert(tokens > 0);
  assert(bucket != nullptr);
  assert(backend != nullptr);

  bucket->put(tokens);

  backend->release();
  responseListener->onMessageEnd();
}

void HttpClusterRequest::onProtocolError(HttpStatus code,
                                         const std::string& message) {
  responseListener->onProtocolError(code, message);
}

} // namespace client
} // namespace http

template <>
JsonWriter& JsonWriter::value(
    TokenShaper<http::client::HttpClusterRequest> const& value) {
  value.writeJSON(*this);
  return *this;
}

template <>
JsonWriter& JsonWriter::value(
    typename TokenShaper<http::client::HttpClusterRequest>::Node const& value) {
  value.writeJSON(*this);
  return *this;
}

} // namespace xzero
