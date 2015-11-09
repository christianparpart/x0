// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/client/HttpClient.h>
#include <xzero/http/client/HttpTransport.h>
#include <xzero/http/client/Http1Connection.h>
#include <xzero/executor/Scheduler.h>
#include <xzero/io/FileRef.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/logging.h>

#define TRACE(msg...) logTrace("HttpClient", msg)

namespace xzero {
namespace http {
namespace client {

HttpClient::HttpClient(Scheduler* scheduler,
                       RefPtr<EndPoint> endpoint)
    : scheduler_(scheduler),
      endpoint_(endpoint),
      transport_(nullptr),
      responseInfo_(),
      responseBody_() {

  if (endpoint_) {
    transport_.reset(new Http1Connection(this, endpoint_.get(), scheduler_));
  }
}

HttpClient::~HttpClient() {
  // FIXME: ownership of transport
  transport_.release();
}

void HttpClient::send(HttpRequestInfo&& requestInfo,
                      CompletionHandler ch) {
  transport_->send(std::move(requestInfo), ch);
}

void HttpClient::send(HttpRequestInfo&& requestInfo,
                      const std::string& requestBody,
                      CompletionHandler ch) {
  transport_->send(std::move(requestInfo), requestBody, ch);
}

void HttpClient::send(const BufferRef& requestBodyChunk, CompletionHandler ch) {
  transport_->send(requestBodyChunk, ch);
}

void HttpClient::send(Buffer&& requestBodyChunk, CompletionHandler ch) {
  transport_->send(std::move(requestBodyChunk), ch);
}

void HttpClient::send(FileRef&& requestBodyChunk, CompletionHandler ch) {
  transport_->send(std::move(requestBodyChunk), ch);
}

void HttpClient::completed() {
  transport_->completed();
}

Future<HttpClient*> HttpClient::waitForResponse() {
  return promise_.future();
}

const HttpResponseInfo& HttpClient::responseInfo() const noexcept {
  return responseInfo_;
}

const Buffer& HttpClient::responseBody() const noexcept {
  return responseBody_;
}

void HttpClient::onMessageBegin(HttpVersion version, HttpStatus code,
                                const BufferRef& text) {
  TRACE("onMessageBegin($0, $1, $2)", version, code, text);

  responseInfo_.setVersion(version);
  responseInfo_.setStatus(code);
  responseInfo_.setReason(text.str());
}

void HttpClient::onMessageHeader(const BufferRef& name, const BufferRef& value) {
  TRACE("onMessageHeader($0, $1)", name, value);

  responseInfo_.headers().push_back(name.str(), value.str());
}

void HttpClient::onMessageHeaderEnd() {
  TRACE("onMessageHeaderEnd()");
}

void HttpClient::onMessageContent(const BufferRef& chunk) {
  TRACE("onMessageContent() $0 bytes", chunk.size());

  responseBody_ += chunk.str();
}

void HttpClient::onMessageEnd() {
  TRACE("onMessageEnd()");
  promise_.success(this);
}

void HttpClient::onProtocolError(HttpStatus code, const std::string& message) {
  logError("HttpClient", "Protocol Error. $0; $1", code, message);
  promise_.failure(Status::ForeignError);
}

} // namespace client
} // namespace http
} // namespace xzero
