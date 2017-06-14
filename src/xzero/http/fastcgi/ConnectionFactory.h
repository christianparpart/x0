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
namespace fastcgi {

/**
 * Connection factory for FastCGI connections.
 */
class XZERO_HTTP_API ConnectionFactory : public HttpConnectionFactory {
 public:
  ConnectionFactory();

  ConnectionFactory(
      size_t maxRequestUriLength,
      size_t maxRequestBodyLength,
      Duration maxKeepAlive);

  ~ConnectionFactory();

  Duration maxKeepAlive() const noexcept { return maxKeepAlive_; }
  void setMaxKeepAlive(Duration value) { maxKeepAlive_ = value; }

  xzero::Connection* create(Connector* connector, EndPoint* endpoint) override;

 private:
  Duration maxKeepAlive_;
};

} // namespace fastcgi
} // namespace http
} // namespace xzero
