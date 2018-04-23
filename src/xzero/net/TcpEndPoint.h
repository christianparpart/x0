// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/Buffer.h>
#include <xzero/executor/Executor.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/Socket.h>
#include <xzero/thread/Future.h>

#include <atomic>
#include <memory>
#include <optional>
#include <functional>
#include <system_error>

namespace xzero {

class TcpConnector;
class TcpConnection;
class FileView;

/**
 * TCP/IP endpoint, as created by an TcpConnector.
 *
 * @see TcpConnector
 */
class TcpEndPoint : public std::enable_shared_from_this<TcpEndPoint> {
 public:
  using Callback = std::function<void(TcpEndPoint*)>;
  using ProtocolCallback = std::function<void(const std::string&, TcpEndPoint*)>;

  /**
   * Initializes an TcpEndPoint.
   *
   * @param socket system file handle representing the client-side socket to
   *               the server.
   * @param readTimeout read-readiness timeout.
   * @param writeTimeout write-readiness timeout.
   * @param executor Task scheduler used for I/O.
   * @param onEndPointClosed invoked when this socket gets closed.
   */
  TcpEndPoint(Socket&& socket,
              Duration readTimeout,
              Duration writeTimeout,
              Executor* executor,
              Callback onEndPointClosed = Callback{});

  TcpEndPoint(Duration readTimeout,
              Duration writeTimeout,
              Executor* executor,
              Callback onEndPointClosed = Callback{});

  virtual ~TcpEndPoint();

  /**
   * Asynchronousely connects to a remote TCP/IP server.
   *
   * The callee does not block.
   *
   * @param address TCP/IP server address and port.
   * @param connectTimeout timeout until the connect must have been completed.
   */
  void connect(const InetAddress& address,
               Duration connectTimeout,
               std::function<void()> onConnected,
               std::function<void(std::error_code ec)> onFailure);

  /**
   * Native operating system handle to the file descriptor.
   */
  const Socket& handle() const noexcept { return socket_; }

  /**
   * Returns the underlying address family, such as @c AF_INET or @c AF_INET6.
   */
  IPAddress::Family addressFamily() const noexcept { return socket_.addressFamily(); }

  /**
   * Tests whether or not this endpoint is still connected.
   */
  bool isOpen() const noexcept;

  /**
   * Convinience method against @c{isOpen() const}.
   */
  bool isClosed() const noexcept { return isOpen() == false; }

  /**
   * Fully closes this endpoint.
   */
  virtual void close();

  /**
   * Retrieves the connection object associated with this TcpEndPoint.
   */
  TcpConnection* connection() const { return connection_.get(); }

  /**
   * Associates a TcpConnection associated with this TcpEndPoint.
   */
  void setConnection(std::unique_ptr<TcpConnection>&& connection);

  /**
   * Sets whether this endpoint is blocking on I/O or not.
   *
   * @param enable @c true to ensures I/O operations block (default), @c false
   *               otherwise.
   */
  void setBlocking(bool enable);

  /**
   * Sets whether to @c TCP_CORK or not.
   */
  void setCorking(bool enable);

  void setTcpNoDelay(bool enable);

  /**
   * String representation of the object for introspection.
   */
  virtual std::string toString() const;

  /**
   * Fills given @p sink with what we can retrieve from this endpoint.
   *
   * @param sink the target buffer to fill with the bytes received.
   *
   * @return Number of bytes received from this endpoint and written
   *         to this sink.
   */
  virtual size_t read(Buffer* sink);

  size_t readahead(size_t maxBytes);
  size_t readBufferSize() const;

  /**
   * Fills given @p sink with what we can retrieve from this endpoint.
   *
   * @param sink the target buffer to fill with the bytes received.
   * @param count number of bytes to fill at most.
   *
   * @return Number of bytes received from this endpoint and written
   *         to this sink.
   */
  virtual size_t read(Buffer* sink, size_t count);

  /**
   * Flushes given buffer @p source into this endpoint.
   *
   * @param source the buffer to flush into this endpoint.
   *
   * @return Number of actual bytes flushed.
   */
  virtual size_t write(const BufferRef& source);

  /**
   * Flushes file contents behind filedescriptor @p fd into this endpoint.
   *
   * @param fileView a view into the file to be sent.
   *
   * @return Number of actual bytes flushed.
   */
  virtual size_t write(const FileView& source);

  /**
   * Registers an interest on reading input data.
   *
   * When a fill-interest can be satisfied you will be notified via your
   * associated TcpConnection object to process the event.
   *
   * @see TcpConnection::onSelectable()
   */
  virtual void wantRead();

  /**
   * Registers an interest on writing output data.
   *
   * When a flush-interest can be satisfied you will be notified via your
   * associated TcpConnection object to process the event.
   *
   * @see TcpConnection::onSelectable()
   */
  virtual void wantWrite();

  /**
   * Retrieves the timeout before a TimeoutError is thrown when I/O
   * interest cannot be fullfilled.
   */
  Duration readTimeout() const noexcept;

  /**
   * Retrieves the timeout before a TimeoutError is thrown when I/O
   * interest cannot be fullfilled.
   */
  Duration writeTimeout() const noexcept;

  /**
   * Initiates detecting the application protocol and initializes the connection
   * object.
   *
   * @param dataReady indicates whether or not data is already available for read.
   * @param createConnection callback to be invoked when the protocol has been
   * detected. This callback must expected to create the connection object.
   */
  void startDetectProtocol(bool dataReady, ProtocolCallback createConnection);

  std::optional<InetAddress> remoteAddress() const;
  std::optional<InetAddress> localAddress() const;

 protected:
  void onConnectComplete(InetAddress address,
                         std::function<void()> onConnected,
                         std::function<void(std::error_code ec)> onFailure);
  void onDetectProtocol(ProtocolCallback createConnection);
  void fillable();
  void flushable();
  void onTimeout();

 protected:
  Executor::HandleRef io_;
  Executor* executor_;
  Duration readTimeout_;
  Duration writeTimeout_;
  Buffer inputBuffer_;
  size_t inputOffset_;
  Socket socket_;
  bool isCorking_;
  Callback onEndPointClosed_;
  std::unique_ptr<TcpConnection> connection_;
};

inline size_t TcpEndPoint::readBufferSize() const {
  return inputBuffer_.size() - inputOffset_;
}

} // namespace xzero
