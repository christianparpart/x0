// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/Api.h>
#include <xzero/sysconfig.h>
#include <xzero/IdleTimeout.h>
#include <xzero/thread/Future.h>
#include <xzero/net/EndPoint.h>
#include <xzero/net/IPAddress.h>
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
   * @param scheduler the task scheduler to be used for I/O & timeout completion.
   */
  InetEndPoint(int socket, InetConnector* connector, Scheduler* scheduler);

  /**
   * Initializes a client-side InetEndPoint.
   *
   * @param socket system file handle representing the client-side socket to
   *               the server.
   * @param addressFamily TCP/IP address-family,
   *                      that is: @c AF_INET or @c AF_INET6.
   * @param readTimeout read-readiness timeout.
   * @param writeTimeout write-readiness timeout.
   * @param scheduler the task scheduler to be used for I/O & timeout completion.
   */
  InetEndPoint(int socket, int addressFamily,
               Duration readTimeout, Duration writeTimeout,
               Scheduler* scheduler);

  ~InetEndPoint();

  /**
   * Connects asynchronousely to a remote TCP/IP server.
   *
   * @param ipaddr TCP/IP server address.
   * @param port TCP/IP server port.
   * @param timeout connect-timeout and default initialization for i/o timeout
   *                in the resulting InetEndPoint.
   * @param scheduler Task scheduler used for connecting and later passed
   *                  to the created InetEndPoint.
   */
  static Future<std::unique_ptr<InetEndPoint>> connectAsync(
      const IPAddress& ipaddr, int port,
      Duration timeout, Scheduler* scheduler);

  static void connectAsync(
      const IPAddress& ipaddr, int port,
      Duration timeout, Scheduler* scheduler,
      std::function<void(std::unique_ptr<InetEndPoint>&&)> onSuccess,
      std::function<void(Status)> onError);

  static std::unique_ptr<InetEndPoint> connect(
      const IPAddress& ipaddr, int port,
      Duration timeout, Scheduler* scheduler);

  int handle() const noexcept { return handle_; }

  /**
   * Returns the underlying address family, such as @c AF_INET or @c AF_INET6.
   */
  int addressFamily() const noexcept { return addressFamily_; }

  /**
   * Retrieves remote address + port.
   */
  Option<std::pair<IPAddress, int>> remoteAddress() const override;

  /**
   * Retrieves local address + port.
   */
  Option<std::pair<IPAddress, int>> localAddress() const;

  // EndPoint overrides
  bool isOpen() const XZERO_NOEXCEPT override;
  void close() override;
  bool isBlocking() const override;
  void setBlocking(bool enable) override;
  bool isCorking() const override;
  void setCorking(bool enable) override;
  std::string toString() const override;
  size_t fill(Buffer* result) override;
  size_t flush(const BufferRef& source) override;
  size_t flush(int fd, off_t offset, size_t size) override;
  void wantFill() override;
  void wantFlush() override;
  Duration readTimeout() override;
  Duration writeTimeout() override;
  void setReadTimeout(Duration timeout) override;
  void setWriteTimeout(Duration timeout) override;
  Option<IPAddress> remoteIP() const override;

 private:
  void onReadable() XZERO_NOEXCEPT;
  void onWritable() XZERO_NOEXCEPT;

  void fillable();
  void flushable();
  void onTimeout();

 private:
  InetConnector* connector_;
  Scheduler* scheduler_;
  Duration readTimeout_;
  Duration writeTimeout_;
  IdleTimeout idleTimeout_;
  Scheduler::HandleRef io_;
  int handle_;
  int addressFamily_;
  bool isCorking_;
};

} // namespace xzero
