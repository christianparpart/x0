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
  class TcpEndPoint;
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
             RefPtr<TcpEndPoint> upstream,
             Duration keepAlive);

  using CreateEndPoint = std::function<Future<RefPtr<TcpEndPoint>>()>;

  HttpClient(Executor* executor,
             CreateEndPoint endpointCreator,
             Duration keepAlive);

  HttpClient(HttpClient&& other);

  /**
   * Requests the resource @p url with custom headers.
   */
  Future<Response> send(const std::string& method,
                        const Uri& url,
                        const HeaderFieldList& headers = {});

  /**
   * Sends given @p request and returns a Future<Response>.
   */
  Future<Response> send(const Request& request);

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

 private:
  Future<RefPtr<TcpEndPoint>> createTcp(InetAddress addr,
                                        Duration connectTimeout,
                                        Duration readTimeout,
                                        Duration writeTimeout);
  void execute();

  class ResponseBuilder;

 private:
  Executor* executor_;
  CreateEndPoint createEndPoint_;
  Duration keepAlive_;
  RefPtr<TcpEndPoint> endpoint_;

  Request request_;
  HttpListener* listener_;
  bool isListenerOwned_;
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

} // namespace xzero::http::client
