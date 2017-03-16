// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/thread/Future.h>
#include <xzero/net/EndPoint.h>
#include <xzero/net/InetAddress.h>
#include <xzero/executor/Executor.h>
#include <atomic>

namespace xzero {

class InetConnector;

/**
 * TCP/IP endpoint, usually created by an InetConnector.
 */
class XZERO_BASE_API InetEndPoint : public EndPoint {
 public:
  /**
   * Initializes a server-side InetEndPoint.
   *
   * @param socket system file handle representing the server-side socket to
   *               the client.
   * @param executor the task scheduler to be used for I/O & timeout completion.
   */
  InetEndPoint(int socket, InetConnector* connector, Executor* executor);

  /**
   * Initializes a client-side InetEndPoint.
   *
   * @param socket system file handle representing the client-side socket to
   *               the server.
   * @param addressFamily TCP/IP address-family,
   *                      that is: @c AF_INET or @c AF_INET6.
   * @param readTimeout read-readiness timeout.
   * @param writeTimeout write-readiness timeout.
   * @param executor the task scheduler to be used for I/O & timeout completion.
   */
  InetEndPoint(int socket, int addressFamily,
               Duration readTimeout, Duration writeTimeout,
               Executor* executor);

  ~InetEndPoint();

  /**
   * Connects asynchronousely to a remote TCP/IP server.
   *
   * @param inet TCP/IP server address and port.
   * @param timeout connect-timeout and default initialization for i/o timeout
   *                in the resulting InetEndPoint.
   * @param scheduler Task scheduler used for connecting and later passed
   *                  to the created InetEndPoint.
   */
  static Future<RefPtr<EndPoint>> connectAsync(
      const InetAddress& inet,
      Duration connectTimeout, Duration readTimeout, Duration writeTimeout,
      Executor* executor);

  static void connectAsync(
      const InetAddress& inet,
      Duration connectTimeout, Duration readTimeout, Duration writeTimeout,
      Executor* executor,
      std::function<void(RefPtr<EndPoint>)> onSuccess,
      std::function<void(const std::error_code&)> onError);

  static RefPtr<EndPoint> connect(
      const InetAddress& inet,
      Duration connectTimeout, Duration readTimeout, Duration writeTimeout,
      Executor* executor);

  int handle() const noexcept { return handle_; }

  /**
   * Returns the underlying address family, such as @c AF_INET or @c AF_INET6.
   */
  int addressFamily() const noexcept { return addressFamily_; }

  // EndPoint overrides
  bool isOpen() const XZERO_NOEXCEPT override;
  void close() override;
  bool isBlocking() const override;
  void setBlocking(bool enable) override;
  bool isCorking() const override;
  void setCorking(bool enable) override;
  bool isTcpNoDelay() const override;
  void setTcpNoDelay(bool enable) override;
  std::string toString() const override;
  using EndPoint::fill;
  size_t fill(Buffer* sink, size_t count) override;
  size_t flush(const BufferRef& source) override;
  size_t flush(int fd, off_t offset, size_t size) override;
  void wantFill() override;
  void wantFlush() override;
  Duration readTimeout() override;
  Duration writeTimeout() override;
  void setReadTimeout(Duration timeout) override;
  void setWriteTimeout(Duration timeout) override;
  Option<InetAddress> remoteAddress() const override;
  Option<InetAddress> localAddress() const override;

  void startDetectProtocol(bool dataReady);
  void onDetectProtocol();

 private:
  void fillable();
  void flushable();
  void onTimeout();

 private:
  InetConnector* connector_;
  Executor* executor_;
  Duration readTimeout_;
  Duration writeTimeout_;
  Executor::HandleRef io_;
  Buffer inputBuffer_;
  size_t inputOffset_;
  int handle_;
  int addressFamily_;
  bool isCorking_;
};

} // namespace xzero
