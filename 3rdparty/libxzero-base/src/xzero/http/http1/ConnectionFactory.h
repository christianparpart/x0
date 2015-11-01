// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/Api.h>
#include <xzero/sysconfig.h>
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
      size_t maxRequestUriLength,
      size_t maxRequestBodyLength,
      size_t maxRequestCount,
      Duration maxKeepAlive,
      bool corkStream);

  ~ConnectionFactory();

  size_t maxRequestCount() const XZERO_NOEXCEPT { return maxRequestCount_; }
  void setMaxRequestCount(size_t value) { maxRequestCount_ = value; }

  Duration maxKeepAlive() const XZERO_NOEXCEPT { return maxKeepAlive_; }
  void setMaxKeepAlive(Duration value) { maxKeepAlive_ = value; }

  bool corkStream() const noexcept { return corkStream_; }

  ::xzero::Connection* create(Connector* connector, EndPoint* endpoint) override;

 private:
  size_t maxRequestCount_;
  Duration maxKeepAlive_;
  bool corkStream_;
};

}  // namespace http1
}  // namespace http
}  // namespace xzero
