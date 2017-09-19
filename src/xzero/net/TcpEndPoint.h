// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/thread/Future.h>
#include <xzero/net/InetAddress.h>
#include <xzero/executor/Executor.h>
#include <atomic>

namespace xzero {

class TcpConnector;
class Connection;
class FileView;

/**
 * TCP/IP endpoint, as created by an TcpConnector.
 *
 * @see TcpConnector
 */
class TcpEndPoint : public RefCounted {
 public:
  using Callback = std::function<void(TcpEndPoint*)>;
  using ProtocolCallback = std::function<void(const std::string&, TcpEndPoint*)>;

  /**
   * Initializes an TcpEndPoint.
   *
   * @param socket system file handle representing the client-side socket to
   *               the server.
   * @param addressFamily TCP/IP address-family,
   *                      that is: @c AF_INET or @c AF_INET6.
   * @param readTimeout read-readiness timeout.
   * @param writeTimeout write-readiness timeout.
   * @param executor the task scheduler to be used for I/O & timeout completion.
   * @param onEndPointClosed invoked when this socket gets closed.
   */
  TcpEndPoint(int socket, int addressFamily,
              Duration readTimeout, Duration writeTimeout,
              Executor* executor,
              Callback onEndPointClosed);

  ~TcpEndPoint();

  /**
   * Asynchronousely connects to a remote TCP/IP server.
   *
   * The callee does not block.
   *
   * @param inet TCP/IP server address and port.
   * @param connectTimeout timeout until the connect must have been completed.
   * @param readTimeout TcpEndPoint-read timeout.
   * @param writeTimeout TcpEndPoint-write timeout.
   * @param scheduler Task scheduler used for connecting and later passed
   *                  to the created TcpEndPoint.
   * @param success Callback to be invoked upon success.
   * @param failure Callback to be invoked upon failure.
   */
  static void connectAsync(
      const InetAddress& inet,
      Duration connectTimeout, Duration readTimeout, Duration writeTimeout,
      Executor* executor,
      std::function<void(RefPtr<TcpEndPoint>)> onSuccess,
      std::function<void(std::error_code)> onError);

  /**
   * Asynchronousely connects to a remote TCP/IP server.
   *
   * The callee does not block.
   *
   * @return A future to the yet to be created TcpEndPoint.
   *
   * @param inet TCP/IP server address and port.
   * @param connectTimeout timeout until the connect must have been completed.
   * @param readTimeout TcpEndPoint-read timeout.
   * @param writeTimeout TcpEndPoint-write timeout.
   * @param scheduler Task scheduler used for connecting and later passed
   *                  to the created TcpEndPoint.
   */
  static Future<RefPtr<TcpEndPoint>> connectAsync(
      const InetAddress& inet,
      Duration connectTimeout, Duration readTimeout, Duration writeTimeout,
      Executor* executor);

  /**
   * Synchronousely connects to a remote TCP/IP server.
   *
   * The callee does not block.
   *
   * @return A future to the yet to be created TcpEndPoint.
   *
   * @param inet TCP/IP server address and port.
   * @param connectTimeout timeout until the connect must have been completed.
   * @param readTimeout TcpEndPoint-read timeout.
   * @param writeTimeout TcpEndPoint-write timeout.
   * @param scheduler Task scheduler used for connecting and later passed
   *                  to the created TcpEndPoint.
   */
  static RefPtr<TcpEndPoint> connect(
      const InetAddress& inet,
      Duration connectTimeout, Duration readTimeout, Duration writeTimeout,
      Executor* executor);

  /**
   * Native operating system handle to the file descriptor.
   */
  int handle() const noexcept { return handle_; }

  /**
   * Returns the underlying address family, such as @c AF_INET or @c AF_INET6.
   */
  int addressFamily() const noexcept { return addressFamily_; }

  /**
   * Tests whether or not this endpoint is still connected.
   */
  bool isOpen() const noexcept;

  /**
   * Convinience method against @c{isOpen() const}.
   */
  bool isClosed() const { return isOpen() == false; }

  /**
   * Fully closes this endpoint.
   */
  virtual void close();

  /**
   * Retrieves the connection object associated with this TcpEndPoint.
   */
  Connection* connection() const { return connection_.get(); }

  /**
   * Associates a Connection associated with this TcpEndPoint.
   */
  void setConnection(std::unique_ptr<Connection>&& connection);

  /**
   * Associates a Connection associated with this TcpEndPoint.
   */
  template<typename T, typename... Args>
  T* setConnection(Args&&... args);

  /**
   * Tests whether this endpoint is blocking on I/O.
   */
  bool isBlocking() const;

  /**
   * Sets whether this endpoint is blocking on I/O or not.
   *
   * @param enable @c true to ensures I/O operations block (default), @c false
   *               otherwise.
   */
  void setBlocking(bool enable);

  /**
   * Retrieves @c TCP_CORK state.
   */
  bool isCorking() const;

  /**
   * Sets whether to @c TCP_CORK or not.
   */
  void setCorking(bool enable);
  bool isTcpNoDelay() const;
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
  virtual size_t fill(Buffer* sink);

  size_t prefill(size_t maxBytes);
  size_t prefilled() const;

  /**
   * Fills given @p sink with what we can retrieve from this endpoint.
   *
   * @param sink the target buffer to fill with the bytes received.
   * @param count number of bytes to fill at most.
   *
   * @return Number of bytes received from this endpoint and written
   *         to this sink.
   */
  virtual size_t fill(Buffer* sink, size_t count);

  /**
   * Flushes given buffer @p source into this endpoint.
   *
   * @param source the buffer to flush into this endpoint.
   *
   * @return Number of actual bytes flushed.
   */
  virtual size_t flush(const BufferRef& source);

  /**
   * Flushes file contents behind filedescriptor @p fd into this endpoint.
   *
   * @param fileView a view into the file to be sent.
   *
   * @return Number of actual bytes flushed.
   */
  virtual size_t flush(const FileView& source);

  /**
   * Registers an interest on reading input data.
   *
   * When a fill-interest can be satisfied you will be notified via your
   * associated Connection object to process the event.
   *
   * @see Connection::onSelectable()
   */
  virtual void wantFill();

  /**
   * Registers an interest on writing output data.
   *
   * When a flush-interest can be satisfied you will be notified via your
   * associated Connection object to process the event.
   *
   * @see Connection::onSelectable()
   */
  virtual void wantFlush();

  /**
   * Retrieves the timeout before a TimeoutError is thrown when I/O
   * interest cannot be * fullfilled.
   */
  Duration readTimeout();

  /**
   * Retrieves the timeout before a TimeoutError is thrown when I/O
   * interest cannot be * fullfilled.
   */
  Duration writeTimeout();

  /**
   * Sets the timeout to wait for the read-interest before an TimeoutError is thrown.
   */
  void setReadTimeout(Duration timeout);

  /**
   * Sets the timeout to wait for the read-interest before an TimeoutError is thrown.
   */
  void setWriteTimeout(Duration timeout);

  Option<InetAddress> remoteAddress() const;
  Option<InetAddress> localAddress() const;

  void startDetectProtocol(bool dataReady, ProtocolCallback createConnection);

  Executor* executor() const noexcept { return executor_; }

 private:
  void onDetectProtocol(ProtocolCallback createConnection);
  void fillable();
  void flushable();
  void onTimeout();

 protected:
  Executor::HandleRef io_;

 private:
  Executor* executor_;
  Duration readTimeout_;
  Duration writeTimeout_;
  Buffer inputBuffer_;
  size_t inputOffset_;
  int handle_;
  int addressFamily_;
  bool isCorking_;
  std::unique_ptr<Connection> connection_;
  Callback onEndPointClosed_;
};

inline size_t TcpEndPoint::prefilled() const {
  return inputBuffer_.size() - inputOffset_;
}

template<typename T, typename... Args>
inline T* TcpEndPoint::setConnection(Args&&... args) {
  setConnection(std::make_unique<T>(std::forward<Args>(args)...));
  return static_cast<T*>(connection());
}

} // namespace xzero
