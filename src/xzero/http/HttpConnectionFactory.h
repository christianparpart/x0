// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/HttpHandler.h>
#include <xzero/http/HttpDateGenerator.h>
#include <xzero/http/HttpOutputCompressor.h>
#include <memory>

namespace xzero {

class WallClock;
class TcpConnector;
class TcpEndPoint;
class TcpConnection;

namespace http {

/**
 * Base HTTP connection factory.
 *
 * This provides common functionality to all HTTP connection factories.
 */
class HttpConnectionFactory {
 public:
  /**
   * Base initiailization for the HTTP connection factory.
   *
   * @param protocolName HTTP protocol name, e.g. http1, http2, ssl+http1, ...
   * @param maxRequestUriLength maximum number of bytes for the request URI
   * @param maxRequestBodyLength maximum number of bytes for the request body
   */
  HttpConnectionFactory(
      const std::string& protocolName,
      size_t maxRequestUriLength,
      size_t maxRequestBodyLength);

  ~HttpConnectionFactory();

  const std::string& protocolName() const noexcept { return protocolName_; }

  size_t maxRequestUriLength() const noexcept { return maxRequestUriLength_; }
  void setMaxRequestUriLength(size_t value) { maxRequestUriLength_ = value; }

  size_t maxRequestBodyLength() const noexcept { return maxRequestBodyLength_; }
  void setMaxRequestBodyLength(size_t value) { maxRequestBodyLength_ = value; }

  const HttpHandlerFactory& handlerFactory() const noexcept { return handlerFactory_; }
  void setHandlerFactory(HttpHandlerFactory&& factory);

  /** Access to the output compression service. */
  HttpOutputCompressor* outputCompressor() const noexcept;

  /** Access to the @c Date response header generator. */
  HttpDateGenerator* dateGenerator() noexcept;

  virtual std::unique_ptr<TcpConnection> create(TcpConnector* connector,
                                                TcpEndPoint* endpoint) = 0;

 private:
  std::string protocolName_;
  size_t maxRequestUriLength_;
  size_t maxRequestBodyLength_;
  HttpHandlerFactory handlerFactory_;
  std::unique_ptr<HttpOutputCompressor> outputCompressor_;
  HttpDateGenerator dateGenerator_;
};

// {{{ inlines
inline HttpOutputCompressor* HttpConnectionFactory::outputCompressor() const noexcept {
  return outputCompressor_.get();
}

inline HttpDateGenerator* HttpConnectionFactory::dateGenerator() noexcept {
  return &dateGenerator_;
}
// }}}

}  // namespace http
}  // namespace xzero
