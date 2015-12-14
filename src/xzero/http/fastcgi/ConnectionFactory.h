// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/http/Api.h>
#include <xzero/sysconfig.h>
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

  Duration maxKeepAlive() const XZERO_NOEXCEPT { return maxKeepAlive_; }
  void setMaxKeepAlive(Duration value) { maxKeepAlive_ = value; }

  xzero::Connection* create(Connector* connector, EndPoint* endpoint) override;

 private:
  Duration maxKeepAlive_;
};

} // namespace fastcgi
} // namespace http
} // namespace xzero
