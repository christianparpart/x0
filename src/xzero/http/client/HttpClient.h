// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Buffer.h>
#include <xzero/Duration.h>
#include <xzero/RefPtr.h>
#include <xzero/CompletionHandler.h>
#include <xzero/thread/Future.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpListener.h>
#include <xzero/stdtypes.h>
#include <memory>
#include <string>

namespace xzero {

class EndPoint;
class IPAddress;
class Executor;
class FileRef;

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
  HttpClient(Executor* executor, RefPtr<EndPoint> endpoint);
  ~HttpClient();

  void send(const HttpRequestInfo& requestInfo, const BufferRef& requestBody);

  Future<HttpClient*> completed();

  // response message accessor
  const HttpResponseInfo& responseInfo() const noexcept;
  const Buffer& responseBody() const noexcept;

 private:
  // HttpListener overrides
  void onMessageBegin(HttpVersion version, HttpStatus code,
                      const BufferRef& text) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageEnd() override;
  void onProtocolError(HttpStatus code, const std::string& message) override;

 private:
  Executor* executor_;

  RefPtr<EndPoint> endpoint_;
  HttpTransport* transport_;

  HttpResponseInfo responseInfo_;
  Buffer responseBody_;

  Promise<HttpClient*> promise_;
};

} // namespace client
} // namespace http
} // namespace xzero
