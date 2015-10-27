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
#include <xzero/net/Connector.h>
#include <xzero/net/IPAddress.h>
#include <xzero/executor/Executor.h>
#include <xzero/executor/Scheduler.h>
#include <xzero/Duration.h>
#include <xzero/RefPtr.h>
#include <xzero/stdtypes.h>
#include <list>
#include <deque>
#include <mutex>

namespace xzero {

class Connection;
class Executor;
class Scheduler;
class InetEndPoint;
class SslEndPoint;

/**
 * TCP/IP Internet Connector API
 */
class XZERO_BASE_API InetConnector : public Connector {
 public:
  /**
   * Initializes this connector.
   *
   * @param name Describing name for this connector.
   * @param executor Executor service to run handlers on
   * @param scheduler Scheduler service to use for scheduling tasks
   * @param readTimeout timespan indicating how long a connection may for read
   *                    readiness.
   * @param writeTimeout timespan indicating how long a connection wait for
   *                     write readiness.
   * @param tcpFinTimeout Timespan to leave client sockets in FIN_WAIT2 state.
   *                      A value of 0 means to leave it at system default.
   * @param eh exception handler for errors in hooks or during events.
   * @param ipaddress TCP/IP address to listen on
   * @param port TCP/IP port number to listen on
   * @param backlog TCP backlog for this listener.
   * @param reuseAddr Flag indicating @c SO_REUSEADDR.
   * @param reusePort Flag indicating @c SO_REUSEPORT.
   *
   * @throw std::runtime_error on any kind of runtime error.
   */
  InetConnector(const std::string& name, Executor* executor,
                Scheduler* scheduler,
                Duration readTimeout, Duration writeTimeout,
                Duration tcpFinTimeout,
                UniquePtr<ExceptionHandler> eh,
                const IPAddress& ipaddress, int port, int backlog,
                bool reuseAddr, bool reusePort);

  /**
   * Minimal initializer.
   *
   * @param name Describing name for this connector.
   * @param executor Executor service to run on
   * @param scheduler Scheduler service to use for timeout management
   * @param readTimeout timespan indicating how long a connection may for read
   *                    readiness.
   * @param writeTimeout timespan indicating how long a connection wait for
   *                     write readiness.
   * @param tcpFinTimeout Timespan to leave client sockets in FIN_WAIT2 state.
   *                      A value of 0 means to leave it at system default.
   * @param eh exception handler for errors in hooks or during events.
   */
  InetConnector(const std::string& name, Executor* executor,
                Scheduler* scheduler,
                Duration readTimeout, Duration writeTimeout,
                Duration tcpFinTimeout,
                UniquePtr<ExceptionHandler> eh);

  ~InetConnector();

  Scheduler* scheduler() const XZERO_NOEXCEPT;

  /**
   * Opens this connector by binding to the given @p ipaddress and @p port.
   *
   * @param ipaddress TCP/IP address to listen on
   * @param port TCP/IP port number to listen on
   * @param backlog TCP backlog for this listener.
   * @param reuseAddr Flag indicating @c SO_REUSEADDR.
   * @param reusePort Flag indicating @c SO_REUSEPORT.
   *
   * @throw std::runtime_error on any kind of runtime error.
   */
  void open(const IPAddress& ipaddress, int port, int backlog, bool reuseAddr,
            bool reusePort);

  /**
   * Tests whether this connector is open.
   */
  bool isOpen() const XZERO_NOEXCEPT;

  /**
   * Implicitely stops and finally closes this connnector.
   */
  void close();

  /**
   * Retrieves the underlying system socket handle.
   */
  int handle() const XZERO_NOEXCEPT;

  /**
   * Returns the IP address family, such as @c IPAddress::V4 or @c IPAddress::V6.
   */
  int addressFamily() const { return addressFamily_; }

  /**
   * Sets the underlying system socket handle.
   */
  void setSocket(int socket);

  size_t backlog() const XZERO_NOEXCEPT;
  void setBacklog(size_t enable);

  /** Tests wether this connector is blocking on accepting new clients. */
  bool isBlocking() const;

  /**
   * Specifies whether accepting new clients should block or not.
   *
   * @param enable true, if accepting a new client should block, false otherwise
   *
   * This flag is inherited by newly created client endpoints as effecient
   * as possible.
   *
   * That is, a non-blocking connector will create non-blocking endpoints
   * for the newly accepted clients.
   *
   * @see EndPoint::setBlocking(bool enable)
   */
  void setBlocking(bool enable);

  /**
   * Tests whether the underlying system handle is closed on exec() syscalls.
   */
  bool closeOnExec() const;

  /**
   * Enables/disables the auto-close flag on exec()-family system calls.
   *
   * This flag is inherited by newly created client endpoints as effecient
   * as possible.
   */
  void setCloseOnExec(bool enable);

  /** Tests whether the underlying system @c TCP_DEFER_ACCEPT flag is set. */
  bool deferAccept() const;

  /** Enables/disables the @c TCP_DEFER_ACCEPT flag on this connector. */
  void setDeferAccept(bool enable);

  /** Tests whether the underlying system @c TCP_QUICKACK flag is set. */
  bool quickAck() const;

  /** Enables/disables the @c TCP_QUICKACK flag on this connector. */
  void setQuickAck(bool enable);

  /** Tests whether the underlying system @c SO_REUSEPORT flag is set. */
  bool reusePort() const;

  /** Enables/disables the @c SO_REUSEPORT flag on this connector. */
  void setReusePort(bool enable);

  /** Tests whether the underlying system @c SO_REUSEADDR flag is set. */
  bool reuseAddr() const;

  /** Enables/disables the @c SO_REUSEADDR flag on this connector. */
  void setReuseAddr(bool enable);

  /**
   * Retrieves the number of maximum attempts to accept a new clients in a row.
   */
  size_t multiAcceptCount() const XZERO_NOEXCEPT;

  /**
   * Sets the number of attempts to accept a new client in a row.
   */
  void setMultiAcceptCount(size_t value) XZERO_NOEXCEPT;

  /**
   * Retrieves the timespan a connection may be idle within an I/O operation.
   */
  Duration readTimeout() const XZERO_NOEXCEPT;

  /**
   * Retrieves the timespan a connection may be idle within an I/O operation.
   */
  Duration writeTimeout() const XZERO_NOEXCEPT;

  /**
   * Sets the timespan a connection may be idle within a read-operation.
   */
  void setReadTimeout(Duration value);

  /**
   * Sets the timespan a connection may be idle within a write-operation.
   */
  void setWriteTimeout(Duration value);

  /**
   * Timespan for FIN_WAIT2 states of the client sockets.
   *
   * A value of 0 means to use the system default.
   */
  Duration tcpFinTimeout() const XZERO_NOEXCEPT;

  /**
   * Sets the timespan to leave a closing client connection in FIN_WAIT2 state.
   *
   * A value of 0 means to use the system-wide default.
   */
  void setTcpFinTimeout(Duration value);

  void start() override;
  bool isStarted() const XZERO_NOEXCEPT override;
  void stop() override;
  std::list<RefPtr<EndPoint>> connectedEndPoints() override;

 private:
  /**
   * Registers to the Scheduler API for new incoming connections.
   */
  void notifyOnEvent();

  /**
   * Accepts up to exactly one new client.
   *
   * This call is blocking by default and can be changed to non-blocking
   * mode via @c setBlocking(bool).
   *
   * @see isBlocking() const
   * @see setBlocking(bool enable)
   */
  int acceptOne();

  /**
   * Creates an EndPoint instance for given client file descriptor.
   */
  virtual RefPtr<EndPoint> createEndPoint(int cfd);

  /**
   * By default, creates Connection from default connection factory and initiates it.
   *
   * Initiated via @c Connection::onOpen().
   */
  virtual void onEndPointCreated(const RefPtr<EndPoint>& endpoint);

  /**
   * Accepts as many pending connections as possible.
   */
  void onConnect();

  void bind(const IPAddress& ipaddr, int port);
  void listen(int backlog);

  /**
   * Invoked by InetEndPoint to inform its creator that it got close()'d.
   *
   * This will unlink the endpoint with this connector, and also notify the
   * endpoint's connection about the event via Connection::onClose().
   *
   * @see Connection::onClose()
   * @see ConnectionListener::onClosed(Connection*)
   */
  void onEndPointClosed(EndPoint* endpoint);
  friend class InetEndPoint;
  friend class SslEndPoint;

 private:
  Scheduler* scheduler_;
  Scheduler::HandleRef schedulerHandle_;

  IPAddress bindAddress_;
  int port_;

  /** Hook invokation wrapper to catch unhandled exceptions. */
  SafeCall safeCall_;

  std::list<RefPtr<EndPoint>> connectedEndPoints_;
  std::mutex mutex_;
  int socket_;
  int addressFamily_;
  int typeMask_;
  int flags_;
  bool blocking_;
  size_t backlog_;
  size_t multiAcceptCount_;
  Duration readTimeout_;
  Duration writeTimeout_;
  Duration tcpFinTimeout_;
  bool isStarted_;
};

inline Scheduler* InetConnector::scheduler() const XZERO_NOEXCEPT {
  return scheduler_;
}

inline Duration InetConnector::readTimeout() const XZERO_NOEXCEPT {
  return readTimeout_;
}

inline Duration InetConnector::writeTimeout() const XZERO_NOEXCEPT {
  return writeTimeout_;
}

inline Duration InetConnector::tcpFinTimeout() const XZERO_NOEXCEPT {
  return tcpFinTimeout_;
}

}  // namespace xzero
