// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/io/FileDescriptor.h>
#include <xzero/Duration.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/TcpUtil.h>
#include <xzero/DeadlineTimer.h>
#include <string>
#include <vector>
#include <system_error>
#include <openssl/ssl.h>

namespace xzero {

class Connection;
class SslConnector;
class SslContext;

class SslErrorCategory : public std::error_category {
 public:
  const char* name() const noexcept override;
  std::string message(int ev) const override;

  static std::error_category& get();
};

inline std::error_code makeSslError(int ec) {
  return std::error_code(ec, SslErrorCategory::get());
}

/**
 * SSL TcpEndPoint, aka SSL socket.
 */
class SslEndPoint : public TcpEndPoint {
 public:
  /**
   * Creates a client-SSL session by connecting to @p target and initiating SSL.
   *
   * @param target Target TCP/IP address & port to connect to
   * @param executor Executor API to use for any underlying I/O task execution.
   * @param sni Server-Name-Indication string to pass; usually matches the DNS host name
   * @param applicationProtocolsSupported list of supported application
   *                                      protocols, such as "http", or "h2"
   * @param createApplicationConnection Factory method for instanciating the
   *                                    application protocol
   */
  static Future<RefPtr<SslEndPoint>> connect(
      const InetAddress& target,
      Executor* executor,
      const std::string& sni,
      const std::vector<std::string>& applicationProtocolsSupported,
      ProtocolCallback createApplicationConnection) {
    return connect(target, 10_seconds, 60_seconds, 60_seconds,
        executor, sni, applicationProtocolsSupported, createApplicationConnection);
  }

  /**
   * Connects to @p target and starts SSL on the connected session.
   *
   * @param target Target TCP/IP address & port to connect to
   * @param connectTimeout Accepted time to wait for the TCP/IP connect to complete
   * @param readTimeout Accepted time to wait for any network read
   * @param writeTimeout Accepted time to wait for any network write to complete
   * @param executor Executor API to use for any underlying I/O task execution.
   * @param sni Server-Name-Indication string to pass; usually matches the DNS host name
   * @param applicationProtocolsSupported list of supported application
   *                                      protocols, such as "http", or "h2"
   * @param createApplicationConnection Factory method for instanciating the
   *                                    application protocol
   */
  static Future<RefPtr<SslEndPoint>> connect(
      const InetAddress& target,
      Duration connectTimeout,
      Duration readTimeout,
      Duration writeTimeout,
      Executor* executor,
      const std::string& sni,
      const std::vector<std::string>& applicationProtocolsSupported,
      ProtocolCallback createApplicationConnection);

  /**
   * Connects to target and starts SSL on the connected session.
   *
   * @param fd file descriptor for the underlying network I/O.
   * @param addressFamily Network address family of given file descriptor @p fd
   * @param readTimeout Accepted time to wait for any network read
   * @param writeTimeout Accepted time to wait for any network write to complete
   * @param executor Executor API to use for any underlying I/O task execution.
   * @param sni Server-Name-Indication string to pass; usually matches the DNS host name
   * @param applicationProtocolsSupported list of supported application
   *                                      protocols, such as "http", or "h2"
   * @param createApplicationConnection Factory method for instanciating the
   *                                    application protocol
   */
  static Future<RefPtr<SslEndPoint>> start(
      FileDescriptor&& fd,
      int addressFamily,
      Duration readTimeout,
      Duration writeTimeout,
      Executor* executor,
      const std::string& sni,
      const std::vector<std::string>& applicationProtocolsSupported,
      ProtocolCallback createApplicationConnection);

  // client initializer
  SslEndPoint(FileDescriptor&& fd,
              int addressFamily,
              Duration readTimeout,
              Duration writeTimeout,
              Executor* executor,
              const std::string& sni,
              const std::vector<std::string>& applicationProtocolsSupported,
              ProtocolCallback connectionFactory);

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
              ProtocolCallback connectionFactory,
              std::function<void(TcpEndPoint*)> onEndPointClosed,
              Executor* executor);

  ~SslEndPoint();

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

  static Buffer makeProtocolList(const std::vector<std::string>& protos);

 private:
  void onClientHandshake(Promise<RefPtr<SslEndPoint>> promise);
  void onClientHandshakeDone();
  void onServerHandshake();
  void fillable();
  void flushable();
  void onTimeout();

  friend class SslConnector;

  enum class Desire { None, Read, Write };

  static int onVerifyCallback(int ok, X509_STORE_CTX *ctx);

  static void tlsext_debug_cb(
      SSL* ssl, int client_server, int type,
      unsigned char* data, int len, SslEndPoint* self);

 private:
  SSL* ssl_;
  Desire bioDesire_;
  ProtocolCallback connectionFactory_;
};

} // namespace xzero

