// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/http/fastcgi/RequestParser.h>
#include <xzero/http/fastcgi/Generator.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/http/HttpTransport.h>
#include <xzero/http/HttpHandler.h>
#include <xzero/Buffer.h>
#include <xzero/Duration.h>
#include <memory>
#include <unordered_map>
#include <list>

namespace xzero {
namespace http {

class HttpDateGenerator;
class HttpOutputCompressor;
class HttpChannel;
class HttpListener;

namespace fastcgi {

class HttpFastCgiChannel;
class HttpFastCgiTransport;

/**
 * @brief Implements a HTTP/1.1 transport connection.
 */
class XZERO_HTTP_API Connection : public ::xzero::Connection {
  friend class HttpFastCgiTransport;
 public:
  Connection(EndPoint* endpoint,
             Executor* executor,
             const HttpHandler& handler,
             HttpDateGenerator* dateGenerator,
             HttpOutputCompressor* outputCompressor,
             size_t maxRequestUriLength,
             size_t maxRequestBodyLength,
             Duration maxKeepAlive);
  ~Connection();

  HttpChannel* createChannel(int request);
  void removeChannel(int request);

  bool isPersistent() const noexcept { return persistent_; }
  void setPersistent(bool enable);

  // xzero::net::Connection overrides
  void onOpen(bool dataReady) override;

 private:
  void parseFragment();

  void onFillable() override;
  void onFlushable() override;
  void onInterestFailure(const std::exception& error) override;
  void onResponseComplete(bool succeed);

  HttpListener* onCreateChannel(int request, bool keepAlive);
  void onUnknownPacket(int request, int record);
  void onAbortRequest(int request);

 private:
  HttpHandler handler_;
  size_t maxRequestUriLength_;
  size_t maxRequestBodyLength_;
  HttpDateGenerator* dateGenerator_;
  HttpOutputCompressor* outputCompressor_;
  Duration maxKeepAlive_;
  Buffer inputBuffer_;
  size_t inputOffset_;
  bool persistent_;
  RequestParser parser_;

  std::unordered_map<int, std::unique_ptr<HttpFastCgiChannel>> channels_;

  EndPointWriter writer_;
  std::list<CompletionHandler> onComplete_;
};

} // namespace fastcgi
} // namespace http
} // namespace xzero
