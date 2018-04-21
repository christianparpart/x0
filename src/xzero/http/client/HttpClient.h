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
#include <xzero/Duration.h>
#include <xzero/CustomDataMgr.h>
#include <xzero/CompletionHandler.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpListener.h>
#include <xzero/net/TcpEndPoint.h>
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
}

namespace xzero::http::client {

class HttpTransport;

/**
 * HTTP client API for a single HTTP message exchange.
 *
 * This API can handle multiple requests at the same time.
 *
 * <ul>
 *   <li>For HTTP/1.1 for each new request a TCP connection is being established.
 *   <li>For HTTP/2, FastCGI, multiplexing may be the preferred option.
 * </ul>
 */
class HttpClient : public CustomData {
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
  class Context;
  class ResponseBuilder;

  void releaseContext(Context* ctx);

 private:
  Executor* executor_;
  InetAddress address_;
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;
  const Duration keepAlive_;

  std::list<std::unique_ptr<Context>> contexts_;
};

class HttpClient::Context {
 public:
  Context(Executor* executor,
          Duration readTimeout,
          Duration writeTimeout,
          std::function<void(Context*)> done,
          const Request& req,
          HttpListener* resp);

  Context(Executor* executor,
          Duration readTimeout,
          Duration writeTimeout,
          std::function<void(Context*)> done,
          const Request& req,
          std::unique_ptr<HttpListener> resp);

  void execute(const InetAddress& address, Duration connectTimeout);

 private:
  void onConnected();
  void onFailure(const std::error_code& ec);

 private:
  Executor* executor_;
  TcpEndPoint endpoint_;
  std::function<void(Context*)> done_;
  Request request_;
  HttpListener* listener_;
  std::unique_ptr<HttpListener> ownedListener_;
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
