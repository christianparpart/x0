// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Duration.h>
#include <xzero/defines.h>
#include <xzero/executor/Executor.h> // for Executor::HandleRef
#include <xzero/net/IPAddress.h>
#include <xzero/net/Socket.h>
#include <xzero/net/TcpConnection.h>
#include <xzero/sysconfig.h>

#include <deque>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace xzero {

class TcpConnection;
class TcpEndPoint;
class SslEndPoint;
class Buffer;

/**
 * TCP/IP Internet Connector API
 */
class TcpConnector {
 public:
  //! Must be a non-printable ASCII byte.
  enum { MagicProtocolSwitchByte = 0x01 };

  enum { RandomPort = 0 };

  typedef std::function<Executor*()> ExecutorSelector;

  /**
   * Creates a new TcpConnection instance for the given @p connector
   * and @p endpoint.
   *
   * @param connector the Connector that accepted the incoming connection.
   * @param endpoint the endpoint that corresponds to this connection.
   *
   * @return pointer to the newly created TcpConnection instance.
   *
   * The newly created TcpConnection instance will be owned by its TcpEndPoint.
   */
  typedef std::function<std::unique_ptr<TcpConnection>(TcpConnector*, TcpEndPoint*)>
      ConnectionFactory;

  /**
   * Initializes this connector.
   *
   * @param name Describing name for this connector.
   * @param executor Executor service to run handlers on
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
  TcpConnector(const std::string& name,
               Executor* executor,
               ExecutorSelector clientExecutorSelector,
               Duration readTimeout,
               Duration writeTimeout,
               Duration tcpFinTimeout,
               const IPAddress& ipaddress, int port, int backlog,
               bool reuseAddr, bool reusePort);

  /**
   * Minimal initializer.
   *
   * @param name Describing name for this connector.
   * @param executor Executor service to run on
   * @param readTimeout timespan indicating how long a connection may for read
   *                    readiness.
   * @param writeTimeout timespan indicating how long a connection wait for
   *                     write readiness.
   * @param tcpFinTimeout Timespan to leave client sockets in FIN_WAIT2 state.
   *                      A value of 0 means to leave it at system default.
   * @param eh exception handler for errors in hooks or during events.
   */
  TcpConnector(const std::string& name,
               Executor* executor,
               ExecutorSelector clientExecutorSelector,
               Duration readTimeout,
               Duration writeTimeout,
               Duration tcpFinTimeout);

  virtual ~TcpConnector();

  Executor* scheduler() const noexcept;

  /**
   * Retrieves the describing name for this connector.
   */
  const std::string& name() const;

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
  bool isOpen() const noexcept;

  /**
   * Retrieves the underlying system socket handle.
   */
  int handle() const noexcept;

  /**
   * Returns the IP address family, such as @c IPAddress::V4 or @c IPAddress::V6.
   */
  Socket::AddressFamily addressFamily() const { return socket_.addressFamily(); }

  size_t backlog() const noexcept;
  void setBacklog(size_t enable);

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
   * @see TcpEndPoint::setBlocking(bool enable)
   */
  void setBlocking(bool enable);

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

  /** Enables/disables the @c TCP_QUICKACK flag on this connector. */
  void setQuickAck(bool enable);

  /** Enables/disables the @c SO_REUSEPORT flag on this connector. */
  void setReusePort(bool enable);

  /** Tests whether TCP-Port reusing is actually supported. */
  static bool isReusePortSupported();

  /** Tests whether TCP_DEFER_ACCEPT is actually supported. */
  static bool isDeferAcceptSupported();

  /** Tests whether the underlying system @c SO_REUSEADDR flag is set. */
  bool reuseAddr() const;

  /** Enables/disables the @c SO_REUSEADDR flag on this connector. */
  void setReuseAddr(bool enable);

  /**
   * Retrieves the number of maximum attempts to accept a new clients in a row.
   */
  size_t multiAcceptCount() const noexcept;

  /**
   * Sets the number of attempts to accept a new client in a row.
   */
  void setMultiAcceptCount(size_t value) noexcept;

  /**
   * Retrieves the timespan a connection may be idle within an I/O operation.
   */
  Duration readTimeout() const noexcept;

  /**
   * Retrieves the timespan a connection may be idle within an I/O operation.
   */
  Duration writeTimeout() const noexcept;

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
  Duration tcpFinTimeout() const noexcept;

  /**
   * Sets the timespan to leave a closing client connection in FIN_WAIT2 state.
   *
   * A value of 0 means to use the system-wide default.
   */
  void setTcpFinTimeout(Duration value);

  /**
   * Starts given connector.
   *
   * @throw std::runtime_error on runtime errors
   */
  void start();

  /**
   * Tests whether this connector has been started.
   */
  bool isStarted() const noexcept;

  /**
   * Stops given connector.
   */
  void stop();

  /**
   * Retrieves list of currently connected endpoints.
   */
  std::list<std::shared_ptr<TcpEndPoint>> connectedEndPoints();

  /**
   * Registeres a new connection factory.
   */
  virtual void addConnectionFactory(const std::string& protocol, ConnectionFactory factory);

  const IPAddress& bindAddress() const noexcept;
  int port() const noexcept;

  /**
   * Creates a TcpConnection object and assigns it to the @p endpoint.
   *
   * When no connection factory is matching the @p protocolName, then
   * the default connection factory will be used instead.
   *
   * @param protocolName The connection's protoclName.
   * @param endpoint The endpoint to assign the newly created connection to.
   */
  void createConnection(const std::string& protocolName, TcpEndPoint* endpoint);

  /** Retrieves all registered connection factories. */
  std::vector<std::string> connectionFactories() const;

  /** Retrieves number of registered connection factories. */
  size_t connectionFactoryCount() const;

  /**
   * Sets the default connection factory.
   */
  void setDefaultConnectionFactory(const std::string& protocolName);

  /**
   * Retrieves the default connection factory.
   */
  ConnectionFactory defaultConnectionFactory() const;

  void loadConnectionFactorySelector(const std::string& protocolName, Buffer* sink);

  /**
   * Retrieves the default task executor service.
   */
  Executor* executor() const { return executor_; }

  std::string toString() const;

 private:
  /**
   * Retrieves associated connection factory by @p protocolName.
   *
   * @param protocolName protocol name for the connection factory to retrieve.
   */
  ConnectionFactory connectionFactory(const std::string& protocolName) const;

  /**
   * Registers to the Executor API for new incoming connections.
   */
  void notifyOnEvent();

  /**
   * Accepts up to exactly one new client.
   *
   * This call is blocking by default and can be changed to non-blocking
   * mode via @c setBlocking(bool).
   *
   * @see setBlocking(bool enable)
   */
  std::optional<Socket> acceptOne();

  /**
   * Creates an TcpEndPoint instance for given client file descriptor.
   *
   * @param cfd       client's file descriptor
   * @param executor  client's designated I/O scheduler
   */
  virtual std::shared_ptr<TcpEndPoint> createEndPoint(Socket&& cfd, Executor* executor);

  /**
   * By default, creates TcpConnection from default connection factory and initiates it.
   *
   * Initiated via @c TcpConnection::onOpen().
   */
  virtual void onEndPointCreated(std::shared_ptr<TcpEndPoint> endpoint);

  /**
   * Accepts as many pending connections as possible.
   */
  void onConnect();

  void bind(const IPAddress& ipaddr, int port);
  void listen(int backlog);

  /**
   * Invoked by TcpEndPoint to inform its creator that it got close()'d.
   */
  void onEndPointClosed(TcpEndPoint* endpoint);
  friend class TcpEndPoint;
  friend class SslConnector;

 private:
  std::string name_;
  Executor* executor_;

  std::unordered_map<std::string, ConnectionFactory> connectionFactories_;
  std::string defaultConnectionFactory_;

  Executor::HandleRef io_;
  ExecutorSelector selectClientExecutor_;

  InetAddress address_;

  std::list<std::shared_ptr<TcpEndPoint>> connectedEndPoints_;
  std::mutex mutex_;
  Socket socket_;
  int typeMask_;
  int flags_;
  bool blocking_;
  size_t backlog_;
  size_t multiAcceptCount_;
  bool deferAccept_;
  Duration readTimeout_;
  Duration writeTimeout_;
  Duration tcpFinTimeout_;
  bool isStarted_;
  bool inDestructor_;
};

inline const IPAddress& TcpConnector::bindAddress() const noexcept {
  return address_.ip();
}

inline int TcpConnector::port() const noexcept {
  return address_.port();
}

inline Duration TcpConnector::readTimeout() const noexcept {
  return readTimeout_;
}

inline Duration TcpConnector::writeTimeout() const noexcept {
  return writeTimeout_;
}

inline Duration TcpConnector::tcpFinTimeout() const noexcept {
  return tcpFinTimeout_;
}

}  // namespace xzero
