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
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpListener.h>
#include <xzero/thread/Future.h>
#include <functional>
#include <vector>
#include <utility>
#include <memory>
#include <string>
#include <deque>

namespace xzero {
  class EndPoint;
  class InetAddress;
  class Executor;
  class FileView;
}

namespace xzero::http {
  class HeaderFieldList;
  class HttpRequest;
}

namespace xzero::http::client {

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
class HttpClient {
 public:
  using Request = HttpRequest;
  using ResponseListener = HttpListener;
  class Response;

  HttpClient(Executor* executor,
             const InetAddress& upstream);

  HttpClient(Executor* executor,
             const InetAddress& upstream,
             Duration connectTimeout,
             Duration readTimeout,
             Duration writeTimeout,
             Duration keepAlive);

  HttpClient(Executor* executor,
             RefPtr<EndPoint> upstream,
             Duration readTimeout,
             Duration writeTimeout,
             Duration keepAlive);

  HttpClient(HttpClient&& other);

  /**
   * Sends given @p request and streams back the response 
   * to @p responseListener.
   *
   * The response is considered complete if either @p responseListener's
   * onError or onMessageEnd has been invoked.
   *
   * @param request The request to send.
   * @param responseListener The listener to stream response events to.
   */
  void send(const Request& request,
            HttpListener* responseListener);

  /**
   * Sends given @p request and invokes @p onSuccess once the full response
   * received or @p onFailure upon communication any failure.
   */
  void send(const Request& request,
            std::function<void(const Response&)> onSuccess,
            std::function<void(std::error_code)> onFailure);

  /**
   * Sends given @p request and returns a Future<Response>.
   */
  Future<Response> send(const Request& request);

  /**
   * Requests the resource @p url with custom headers.
   */
  Future<Response> send(const std::string& method,
                        const Uri& url,
                        const HeaderFieldList& headers = {});

 private:
  struct Task {
    HttpClient::Request request;
    HttpListener* listener;
    bool isListenerOwned;
  };

 private:
  bool isClosed() const;
  void startConnect();
  void setupConnection();
  HttpTransport* getChannel();
  bool tryConsumeTask();

  class ResponseBuilder;

 private:
  Executor* executor_;

  InetAddress upstream_;
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;
  Duration keepAlive_;

  RefPtr<EndPoint> endpoint_;
  std::deque<Task> pendingTasks_;
};

class HttpClient::Response : public HttpResponseInfo {
 public:
  Response() = default;
  Response(Response&&) = default;
  Response(const Response&) = default;

  HugeBuffer& content() { return content_; }
  const HugeBuffer& content() const { return content_; }

 private:
  HugeBuffer content_;
};

class HttpClient::ResponseBuilder : public HttpListener {
 public:
  ResponseBuilder(std::function<void(Response&&)> s, std::function<void(std::error_code)> e);

  void onMessageBegin(HttpVersion version, HttpStatus code, const BufferRef& text) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onError(std::error_code ec) override;

 private:
  std::function<void(Response&&)> success_;
  std::function<void(std::error_code)> failure_;
  Response response_;
};

} // namespace xzero::http::client
