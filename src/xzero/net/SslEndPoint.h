// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/DeadlineTimer.h>
#include <openssl/ssl.h>

namespace xzero {

class Connection;
class SslConnector;
class SslContext;

/**
 * SSL TcpEndPoint, aka SSL socket.
 */
class SslEndPoint : public TcpEndPoint {
 public:
  /**
   * Initializes an SSL endpoint.
   *
   * @param fd
   * @param addressFamily
   * @param readTimeout
   * @param writeTimeout
   * @param defaultContext
   * @param connectionFactory
   * @param onEndPointClosed
   * @param executor
   */
  SslEndPoint(FileDescriptor&& fd,
              int addressFamily,
              Duration readTimeout,
              Duration writeTimeout,
              SslContext* defaultContext,
              std::function<void(const std::string&, SslEndPoint*)> connectionFactory,
              std::function<void(TcpEndPoint*)> onEndPointClosed,
              Executor* executor);

  ~SslEndPoint();

  bool isOpen() const noexcept override;
  void close() override;

  void shutdown();

  using TcpEndPoint::fill;
  size_t fill(Buffer* sink, size_t count) override;
  size_t flush(const BufferRef& source) override;
  size_t flush(const FileView& source) override;

  /**
   * Ensures that the SSL socket is ready for receiving data.
   *
   * Any pending data in the fillBuffer will preceed.
   *
   * This might internally cause write <b>and</b> read operations
   * through the SSL layer.
   */
  void wantFill() override;

  /** Ensures that the SSL socket is ready for more flush operations.
   *
   * Any pending data in the flushBuffer will be sent before
   * @c connection()->onFlushable() will be invoked.
   *
   * This might internally cause write <b>and</b> read operations
   * through the SSL layer.
   */
  void wantFlush() override;

  std::string toString() const override;

  /**
   * Retrieves the string that is identifies the negotiated next protocol, such
   * as "HTTP/1.1" or "SPDY/3.1".
   *
   * This method is implemented using ALPN protocol extensions to TLS.
   */
  BufferRef applicationProtocolName() const;

 private:
  void onHandshake();
  void fillable();
  void flushable();
  void onTimeout();

  friend class SslConnector;

  enum class Desire { None, Read, Write };

  static void tlsext_debug_cb(
      SSL* ssl, int client_server, int type,
      unsigned char* data, int len, SslEndPoint* self);

 private:
  SSL* ssl_;
  Desire bioDesire_;
  std::function<void(const std::string&, SslEndPoint*)> connectionFactory_;
};

} // namespace xzero
