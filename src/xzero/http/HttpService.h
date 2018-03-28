// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/HttpConnectionFactory.h>
#include <xzero/Duration.h>
#include <xzero/UnixTime.h> // BuiltinAssetHandler
#include <xzero/Buffer.h>
#include <vector>
#include <unordered_map>

namespace xzero {

class TcpConnector;
class Executor;
class WallClock;
class Scheduler;
class IPAddress;
class HugeBuffer;

namespace http {

class HttpListener;
class HttpRequestInfo;
class HttpRequest;
class HttpResponse;

/**
 * General purpose multi-handler HTTP Service API.
 *
 * @note HTTP/1 is always enabled by default.
 */
class HttpService {
 public:
  using Handler = std::function<bool(HttpRequest*, HttpResponse*)>;
  class BuiltinAssetHandler;

  enum Protocol {
    HTTP1,
    FCGI,
  };

  HttpService();

  explicit HttpService(Protocol protocol);

  HttpService(Executor* executor, int port);
  HttpService(Executor* executor, int port, const IPAddress& bind);
  ~HttpService();

  /**
   * Configures this service to listen on TCP/IP using the given parameters.
   *
   * @param executor the executor service to run tasks on.
   * @param clientExecutor the executor for the client connections.
   * @param readTimeout timespan indicating how long a connection may be idle for read.
   * @param writeTimeout timespan indicating how long a connection may be idle for write.
   * @param tcpFinTimeout Timespan to leave client sockets in FIN_WAIT2 state.
   *                      A value of 0 means to leave it at system default.
   * @param ipaddress the TCP/IP bind address.
   * @param port the TCP/IP port to listen on.
   * @param backlog the number of connections allowed to be queued in kernel.
   *
   */
  TcpConnector* configureTcp(Executor* executor,
                             Executor* clientExecutor,
                             Duration readTimeout,
                             Duration writeTimeout,
                             Duration tcpFinTimeout,
                             const IPAddress& ipaddress,
                             int port,
                             int backlog = 128);

  /** Registers a new @p handler. */
  void addHandler(Handler handler);

  /** Starts the internal server. */
  void start();

  /** Stops the internal server. */
  void stop();

  /**
   * Sends given request to the underlying HTTP service layer.
   *
   * @param request The incoming request to be handled.
   * @param requestBody HTTP request body (if any).
   * @param responseListener callback listener to receive the response
   *                         by the time it is generated.
   */
  void send(const HttpRequestInfo& request,
            HugeBuffer&& requestBody,
            HttpListener* responseListener);

  const std::vector<Handler>& handlers() const noexcept { return handlers_; }

 private:
  static Protocol getDefaultProtocol();
  void attachProtocol(TcpConnector* connector);
  void attachHttp1(TcpConnector* connector);
  void attachFCGI(TcpConnector* connector);
  std::function<void()> createHandler(HttpRequest* request, HttpResponse* response);
  void handleRequest(HttpRequest* request, HttpResponse* response);
  void onAllDataRead(HttpRequest* request, HttpResponse* response);

 private:
  Protocol protocol_;
  std::vector<std::unique_ptr<HttpConnectionFactory>> httpFactories_;
  std::unique_ptr<TcpConnector> inetConnector_;
  std::vector<Handler> handlers_;
};

/**
 * Builtin Asset Handler for HttpService.
 */
class HttpService::BuiltinAssetHandler {
 public:
  BuiltinAssetHandler();

  void addAsset(const std::string& path, const std::string& mimetype,
                const Buffer&& data);

  bool operator()(HttpRequest* request, HttpResponse* response) {
    return handleRequest(request, response);
  }

  bool handleRequest(HttpRequest* request, HttpResponse* response);

 private:
  struct Asset {
    std::string mimetype;
    UnixTime mtime;
    Buffer data;
  };

  std::unordered_map<std::string, Asset> assets_;
};

} // namespace http
} // namespace xzero
