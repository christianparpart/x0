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
#include <xzero/net/InetEndPoint.h>
#include <xzero/net/DnsClient.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/IPAddress.h>
#include <xzero/io/FileView.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

#define TRACE(msg...) logTrace("HttpClient", msg)

namespace xzero {
namespace http {
namespace client {

HttpClient::HttpClient(Executor* executor,
                       RefPtr<EndPoint> endpoint)
    : executor_(executor),
      endpoint_(endpoint),
      transport_(nullptr),
      responseInfo_(),
      responseBody_(),
      promise_() {

  if (endpoint_) {
    // is not being freed here explicitely, as the endpoint will own that
    transport_ = new Http1Connection(this, endpoint_.get(), executor_);
  }
}

HttpClient::HttpClient(HttpClient&& other)
    : executor_(other.executor_),
      endpoint_(std::move(other.endpoint_)),
      transport_(other.transport_),
      responseInfo_(std::move(other.responseInfo_)),
      responseBody_(std::move(other.responseBody_)),
      promise_(std::move(other.promise_)) {
  transport_->setListener(this);
  other.transport_ = nullptr;
}

HttpClient::~HttpClient() {
}

void HttpClient::send(const HttpRequestInfo& requestInfo,
                      const BufferRef& requestBody) {
  transport_->send(requestInfo, nullptr);
  transport_->send(requestBody, nullptr);
}

Future<HttpClient*> HttpClient::completed() {
  transport_->completed();
  promise_ = Some(Promise<HttpClient*>());
  return promise_->future();
}

const HttpResponseInfo& HttpClient::responseInfo() const noexcept {
  return responseInfo_;
}

const Buffer& HttpClient::responseBody() const noexcept {
  return responseBody_;
}

void HttpClient::onMessageBegin(HttpVersion version, HttpStatus code,
                                const BufferRef& text) {
  TRACE("onMessageBegin($0, $1, $2)", version, (int)code, text);

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
  promise_->success(this);
}

void HttpClient::onProtocolError(HttpStatus code, const std::string& message) {
  logError("HttpClient", "Protocol Error. $0; $1", code, message);
  promise_->failure(Status::ForeignError);
}

Future<HttpClient> HttpClient::sendAsync(
    const HttpRequestInfo& requestInfo, const BufferRef& requestBody,
    Executor* executor) {

  std::string host = requestInfo.headers().get("Host");
  if (host.empty())
    RAISE(RuntimeError, "No Host HTTP request header given.");

  std::vector<std::string> parts = StringUtil::split(host, ":");
  host = parts[0];
  int port = parts.size() == 2 ? stoi(parts[1]) : 80;

  std::vector<IPAddress> ipaddresses = DnsClient().ip(host);
  if (ipaddresses.empty())
    RAISE(RuntimeError, "Host header does not resolve to an IP address.");

  Duration connectTimeout = 4_seconds;
  Duration readTimeout = 30_seconds;
  Duration writeTimeout = 8_seconds;

  return sendAsync(InetAddress(ipaddresses.front(), port),
                   requestInfo, requestBody,
                   connectTimeout, readTimeout, writeTimeout, executor);
}

Future<HttpClient> HttpClient::sendAsync(
    const std::string& method,
    const Uri& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const BufferRef& requestBody,
    Executor* executor) {

  HttpRequestInfo requestInfo(HttpVersion::VERSION_1_1, method,
                              url.pathAndQuery(), requestBody.size(),
                              headers);

  std::vector<IPAddress> ipaddresses = DnsClient().ip(url.host());
  if (ipaddresses.empty())
    RAISE(RuntimeError, "Host header does not resolve to an IP address.");

  Duration connectTimeout = 4_seconds;
  Duration readTimeout = 30_seconds;
  Duration writeTimeout = 8_seconds;

  return sendAsync(InetAddress(ipaddresses.front(), url.port()),
                   requestInfo, requestBody,
                   connectTimeout, readTimeout, writeTimeout, executor);
}

Future<HttpClient> HttpClient::sendAsync(
    const InetAddress& inet,
    const HttpRequestInfo& requestInfo,
    const BufferRef& requestBody,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    Executor* executor) {

  Promise<HttpClient> promise;

  Future<RefPtr<EndPoint>> fep = InetEndPoint::connectAsync(inet,
      connectTimeout, readTimeout, writeTimeout, executor);

  fep.onFailure([promise](Status s) mutable {
    promise.failure(s);
  });

  fep.onSuccess([promise, executor, requestInfo, requestBody]
                (const RefPtr<EndPoint>& ep) mutable {
      HttpClient* client = new HttpClient(executor, ep);

      client->send(requestInfo, requestBody);
      Future<HttpClient*> f = client->completed();
      f.onFailure([promise, client](Status s) mutable {
        delete client;
        promise.failure(s);
      });
      f.onSuccess([promise, client](HttpClient*) mutable {
        promise.success(std::move(*client));
        delete client;
      });
  });

  return promise.future();
}

} // namespace client
} // namespace http
} // namespace xzero
