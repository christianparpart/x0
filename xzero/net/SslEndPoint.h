// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/net/EndPoint.h>
#include <xzero/net/InetUtil.h>
#include <xzero/DeadlineTimer.h>
#include <openssl/ssl.h>

namespace xzero {

class SslConnector;
class SslContext;

/**
 * SSL EndPoint, aka SSL socket.
 */
class XZERO_BASE_API SslEndPoint : public EndPoint {
 public:
  SslEndPoint(int socket, SslConnector* connector, Executor* executor);

  /**
   * Initializes an SSL endpoint.
   *
   * @param fd
   * @param readTimeout
   * @param writeTimeout
   */
  SslEndPoint(FileDescriptor&& fd,
              Duration readTimeout,
              Duration writeTimeout,
              SslContext* defaultContext,
              std::function<void(EndPoint*)> onEndPointClosed,
              Executor* executor);

  ~SslEndPoint();

  int handle() const noexcept { return handle_; }

  bool isOpen() const override;
  void close() override;

  /**
   * Closes the connection the hard way, by ignoring the SSL layer.
   */
  void abort();

  using EndPoint::fill;

  /**
   * Reads from remote endpoint and fills given buffer with it.
   */
  size_t fill(Buffer* sink, size_t count) override;

  /**
   * Appends given buffer into the pending buffer vector and attempts to flush.
   */
  size_t flush(const BufferRef& source) override;
  size_t flush(int fd, off_t offset, size_t size) override;

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

  Duration readTimeout() override;
  Duration writeTimeout() override;
  void setReadTimeout(Duration timeout) override;
  void setWriteTimeout(Duration timeout) override;
  bool isBlocking() const override;
  void setBlocking(bool enable) override;
  bool isCorking() const override;
  void setCorking(bool enable) override;
  bool isTcpNoDelay() const override;
  void setTcpNoDelay(bool enable) override;
  std::string toString() const override;

  /**
   * Retrieves the string that is identifies the negotiated next protocol, such
   * as "HTTP/1.1" or "SPDY/3.1".
   *
   * This method is implemented using NPN or ALPN protocol extensions to TLS.
   */
  BufferRef nextProtocolNegotiated() const;

 private:
  void onHandshake();
  void fillable();
  void flushable();
  void shutdown();
  void onTimeout();

  friend class SslConnector;

  enum class Desire { None, Read, Write };

  static void tlsext_debug_cb(
      SSL* ssl, int client_server, int type,
      unsigned char* data, int len, SslEndPoint* self);

 private:
  int handle_;
  bool isCorking_;

  std::function<void(EndPoint*)> onEndPointClosed_;

  Executor* executor_;
  SSL* ssl_;
  Desire bioDesire_;
  Executor::HandleRef io_;
  Duration readTimeout_;
  Duration writeTimeout_;
  DeadlineTimer idleTimeout_;
};

} // namespace xzero
