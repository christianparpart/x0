// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
#include <functional>
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
class HttpRequest;

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
  HttpClient(Executor* executor,
             const InetAddress& upstream,
             Duration connectTimeout,
             Duration writeTimeout,
             Duration readTimeout);

  HttpClient(Executor* executor,
             const InetAddress& upstream);

  HttpClient(HttpClient&& other);

  using Request = HttpRequest;
  using ResponseListener = HttpListener;
  class Response;

  void send(const Request& request,
            std::function<void(Response&&)> onSuccess,
            std::function<void(std::error_code)> onFailure);

  Future<Response> send(const Request& request);

  /**
   * Sends given @p request to @p transport and feeds the response to the
   * callers @p responseListener.
   *
   * @param request
   * @param responseListener
   */
  void send(const Request& request,
            HttpListener* responseListener);

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

  RefPtr<EndPoint> endpoint_;
  HttpTransport* transport_;

  HttpRequestInfo requestInfo_;
  Buffer requestBody_;

  HttpResponseInfo responseInfo_;
  HugeBuffer responseBody_;

  std::unique_ptr<Promise<HttpClient*>> promise_;
};

class HttpClient::Response : public HttpResponseInfo {
 public:
  Response();
  Response(Response&&) = default;
  Response(const Response&) = default;

  HugeBuffer& content() { return content_; }
  const HugeBuffer& content() const { return content_; }

 private:
  HugeBuffer content_;
};
inline const HttpRequestInfo& HttpClient::requestInfo() const noexcept {
  return requestInfo_;
}

} // namespace client
} // namespace http
} // namespace xzero
