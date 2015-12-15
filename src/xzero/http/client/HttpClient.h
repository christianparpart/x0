// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Uri.h>
#include <xzero/Buffer.h>
#include <xzero/HugeBuffer.h>
#include <xzero/RefPtr.h>
#include <xzero/Option.h>
#include <xzero/Duration.h>
#include <xzero/CompletionHandler.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpListener.h>
#include <xzero/thread/Future.h>
#include <xzero/stdtypes.h>
#include <vector>
#include <utility>
#include <memory>
#include <string>

namespace xzero {

class EndPoint;
class InetAddress;
class Executor;
class FileView;

namespace http {

class HeaderFieldList;

namespace client {

class HttpTransport;

/**
 * HTTP client API for a single HTTP message exchange.
 *
 * It can process one message-exchange at a time and can be reused after
 * for more message exchanges.
 *
 * Abstracts away the underlying transport protocol, such as
 * HTTP/1, HTTP/2, HTTPS, FastCGI.
 */
class HttpClient : public HttpListener {
 public:
  explicit HttpClient(Executor* executor);
  HttpClient(HttpClient&& other);
  ~HttpClient();

  // request builder
  const HttpRequestInfo& requestInfo() const noexcept;
  void setRequest(const HttpRequestInfo& requestInfo, const BufferRef& requestBody);
  Future<HttpClient*> sendAsync(InetAddress& addr, Duration connectTimeout,
                                Duration readTimeout, Duration writeTimeout);
  void send(RefPtr<EndPoint> ep);

  // response message accessor
  const HttpResponseInfo& responseInfo() const noexcept;
  bool isResponseBodyBuffered() const noexcept;
  const BufferRef& responseBody();
  FileView takeResponseBody();

 private:
  // HttpListener overrides
  void onMessageBegin(HttpVersion version, HttpStatus code,
                      const BufferRef& text) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onProtocolError(HttpStatus code, const std::string& message) override;

 private:
  Executor* executor_;

  HttpTransport* transport_;

  HttpRequestInfo requestInfo_;
  Buffer requestBody_;

  HttpResponseInfo responseInfo_;
  HugeBuffer responseBody_;

  Promise<HttpClient*> promise_;
};

inline const HttpRequestInfo& HttpClient::requestInfo() const noexcept {
  return requestInfo_;
}

} // namespace client
} // namespace http
} // namespace xzero
