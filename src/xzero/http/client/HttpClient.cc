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

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("HttpClient", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

namespace xzero {
namespace http {
namespace client {

constexpr size_t responseBodyBufferSize = 4 * 1024;

HttpClient::HttpClient(Executor* executor,
                       RefPtr<EndPoint> endpoint)
    : executor_(executor),
      endpoint_(endpoint),
      transport_(nullptr),
      responseInfo_(),
      responseBodyBuffer_(),
      responseBodyFd_(-1),
      responseBodySize_(0),
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
      responseBodyBuffer_(std::move(other.responseBodyBuffer_)),
      responseBodyFd_(std::move(other.responseBodyFd_)),
      responseBodySize_(other.responseBodySize_),
      promise_(std::move(other.promise_)) {
  transport_->setListener(this);
  other.transport_ = nullptr;
  other.responseBodySize_ = 0;
}

HttpClient::~HttpClient() {
}

void HttpClient::send(const HttpRequestInfo& requestInfo,
                      const BufferRef& requestBody) {
  TRACE("send: $0 $1", requestInfo.method(), requestInfo.path());
  transport_->send(requestInfo, nullptr);
  transport_->send(requestBody, nullptr);
}

Future<HttpClient*> HttpClient::completed() {
  TRACE("completed()");
  transport_->completed();
  promise_ = Some(Promise<HttpClient*>());
  return promise_->future();
}

const HttpResponseInfo& HttpClient::responseInfo() const noexcept {
  return responseInfo_;
}

bool HttpClient::isResponseBodyBuffered() const noexcept {
  return responseBodyFd_ < 0;
}

const Buffer& HttpClient::responseBody() {
  if (responseBodyBuffer_.empty() && responseBodyFd_.isOpen())
    responseBodyBuffer_ = FileUtil::read(takeResponseBody());

  return responseBodyBuffer_;
}

FileView HttpClient::takeResponseBody() {
  return FileView(std::move(responseBodyFd_), 0, responseBodySize_);
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
  TRACE("onMessageContent(BufferRef) $0 bytes", chunk.size());

  if (responseBodyBuffer_.size() + chunk.size() > responseBodyBufferSize
      && responseBodyFd_.isClosed()) {
    responseBodyFd_ = FileUtil::createTempFile();
    if (responseBodyFd_.isOpen()) {
      FileUtil::write(responseBodyFd_, responseBodyBuffer_);
      responseBodyBuffer_.clear();
    }
  }

  if (responseBodyFd_.isOpen())
    FileUtil::write(responseBodyFd_, chunk);
  else
    responseBodyBuffer_ += chunk.str();

  responseBodySize_ += chunk.size();
}

void HttpClient::onMessageContent(FileView&& chunk) {
  TRACE("onMessageContent(FileView) $0 bytes", chunk.size());

  if (responseBodyBuffer_.size() + chunk.size() > responseBodyBufferSize
      && responseBodyFd_.isClosed()) {
    responseBodyFd_ = FileUtil::createTempFile();
    if (responseBodyFd_.isOpen()) {
      FileUtil::write(responseBodyFd_, responseBodyBuffer_);
      responseBodyBuffer_.clear();
    }
  }

  if (responseBodyFd_.isOpen())
    FileUtil::write(responseBodyFd_, chunk);
  else
    chunk.fill(&responseBodyBuffer_);

  responseBodySize_ += chunk.size();
}

void HttpClient::onMessageEnd() {
  TRACE("onMessageEnd()");
  responseInfo_.setContentLength(responseBodySize_);
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
