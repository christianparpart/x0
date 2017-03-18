// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/Duration.h>
#include <xzero/http/HttpConnectionFactory.h>

namespace xzero {
namespace http {
namespace http1 {

/**
 * Connection factory for HTTP/1 connections.
 */
class XZERO_HTTP_API ConnectionFactory : public HttpConnectionFactory {
 public:
  ConnectionFactory();

  ConnectionFactory(
      size_t requestHeaderBufferSize,
      size_t requestBodyBufferSize,
      size_t maxRequestUriLength,
      size_t maxRequestBodyLength,
      size_t maxRequestCount,
      Duration maxKeepAlive,
      bool corkStream,
      bool tcpNoDelay);

  ~ConnectionFactory();

  size_t requestHeaderBufferSize() const noexcept { return requestHeaderBufferSize_; }
  void setRequestHeaderBufferSize(size_t value) { requestHeaderBufferSize_ = value; }

  size_t requestBodyBufferSize() const noexcept { return requestBodyBufferSize_; }
  void setRequestBodyBufferSize(size_t value) { requestBodyBufferSize_ = value; }

  size_t maxRequestCount() const noexcept { return maxRequestCount_; }
  void setMaxRequestCount(size_t value) { maxRequestCount_ = value; }

  Duration maxKeepAlive() const noexcept { return maxKeepAlive_; }
  void setMaxKeepAlive(Duration value) { maxKeepAlive_ = value; }

  bool corkStream() const noexcept { return corkStream_; }
  bool tcpNoDelay() const noexcept { return tcpNoDelay_; }

  Connection* create(Connector* connector, EndPoint* endpoint) override;

 private:
  size_t requestHeaderBufferSize_;
  size_t requestBodyBufferSize_;
  size_t maxRequestCount_;
  Duration maxKeepAlive_;
  bool corkStream_;
  bool tcpNoDelay_;
};

}  // namespace http1
}  // namespace http
}  // namespace xzero
