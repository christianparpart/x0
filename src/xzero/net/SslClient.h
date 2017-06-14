// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero/RefPtr.h>
#include <xzero/thread/Future.h>
#include <xzero/net/EndPoint.h>
#include <xzero/io/FileDescriptor.h>
#include <functional>
#include <vector>
#include <string>

#include <openssl/ssl.h>

namespace xzero {

class Executor;

class SslClient : public EndPoint {
 public:
  typedef std::function<Connection*(const std::string&)> ConnectionFactory;

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
  static Future<RefPtr<SslClient>> connect(
      const InetAddress& target,
      Duration connectTimeout,
      Duration readTimeout,
      Duration writeTimeout,
      Executor* executor,
      const std::string& sni,
      const std::vector<std::string>& applicationProtocolsSupported,
      ConnectionFactory createApplicationConnection);

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
  static Future<RefPtr<SslClient>> start(
      FileDescriptor&& fd,
      int addressFamily,
      Duration readTimeout,
      Duration writeTimeout,
      Executor* executor,
      const std::string& sni,
      const std::vector<std::string>& applicationProtocolsSupported,
      ConnectionFactory createApplicationConnection);

 public:
  SslClient(
      FileDescriptor&& fd,
      int addressFamily,
      Duration readTimeout,
      Duration writeTimeout,
      Executor* executor,
      const std::string& sni,
      const std::vector<std::string>& applicationProtocolsSupported,
      ConnectionFactory createApplicationConnection);

  ~SslClient();

  Future<RefPtr<SslClient>> handshake();

  std::string nextProtocolNegotiated() const;

  // EndPoint overrides
  bool isOpen() const override;
  void close() override;
  size_t fill(Buffer* sink, size_t count) override;
  size_t flush(const BufferRef& source) override;
  size_t flush(int fd, off_t offset, size_t size) override;
  void wantFill() override;
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
  Option<InetAddress> remoteAddress() const override;
  Option<InetAddress> localAddress() const override;

 private:
  void onHandshake(Promise<RefPtr<SslClient>> promise);

 private:
  const SSL_METHOD* method_;
  SSL_CTX* ctx_;
  SSL* ssl_;
  FileDescriptor fd_;
  int addressFamily_;

  Duration readTimeout_;
  Duration writeTimeout_;
  Executor* executor_;
  std::function<Connection*(const std::string&)> createApplicationConnection_;
};

} // namespace xzero
