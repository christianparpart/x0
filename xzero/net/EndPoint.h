// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/Buffer.h>
#include <xzero/Duration.h>
#include <xzero/RefCounted.h>
#include <xzero/Option.h>
#include <xzero/net/InetAddress.h>
#include <string>

namespace xzero {

class Buffer;
class BufferRef;
class Connection;

/**
 * A communication endpoint (such as internet sockets, pipes, string streams).
 *
 * The Endpoint implements the raw data transport without application knowledge.
 *
 * An application layer is implemented via the Connection interface that
 * can be associated with its EndPoint.
 *
 * There can be only one Connection object handling this EndPoint.
 *
 * @see ByteArrayEndPoint
 * @see InetEndPoint
 * @see Connection
 */
class EndPoint : public RefCounted {
 private:
  EndPoint(EndPoint&) = delete;
  EndPoint& operator=(EndPoint&) = delete;

 public:
  EndPoint() noexcept;
  ~EndPoint();

  /**
   * Retrieves the connection object associated with this EndPoint.
   */
  Connection* connection() const { return connection_.get(); }

  /**
   * Associates a Connection associated with this EndPoint.
   */
  void setConnection(std::unique_ptr<Connection>&& connection);

  /**
   * Associates a Connection associated with this EndPoint.
   */
  template<typename T, typename... Args>
  T* setConnection(Args&&... args);

  /**
   * Tests whether or not this endpoint is still connected.
   */
  virtual bool isOpen() const = 0;

  /**
   * Convinience method against @c{isOpen() const}.
   */
  bool isClosed() const { return isOpen() == false; }

  /**
   * Fully closes this endpoint.
   */
  virtual void close() = 0;

  size_t fill(Buffer* sink);

  /**
   * Fills given @p sink with what we can retrieve from this endpoint.
   *
   * @param sink the target buffer to fill with the bytes received.
   * @param count number of bytes to fill at most.
   *
   * @return Number of bytes received from this endpoint and written
   *         to this sink.
   */
  virtual size_t fill(Buffer* sink, size_t count) = 0;

  /**
   * Flushes given buffer @p source into this endpoint.
   *
   * @param source the buffer to flush into this endpoint.
   *
   * @return Number of actual bytes flushed.
   */
  virtual size_t flush(const BufferRef& source) = 0;

  /**
   * Flushes file contents behind filedescriptor @p fd into this endpoint.
   *
   * @param fd Handle to a valid opened file.
   * @param offset Offset where to start reading from.
   * @param size Number of bytes to write from the given file.
   *
   * @return Number of actual bytes flushed.
   */
  virtual size_t flush(int fd, off_t offset, size_t size) = 0;

  /**
   * Registers an interest on reading input data.
   *
   * When a fill-interest can be satisfied you will be notified via your
   * associated Connection object to process the event.
   *
   * @see Connection::onSelectable()
   */
  virtual void wantFill() = 0;

  /**
   * Registers an interest on writing output data.
   *
   * When a flush-interest can be satisfied you will be notified via your
   * associated Connection object to process the event.
   *
   * @see Connection::onSelectable()
   */
  virtual void wantFlush() = 0;

  /**
   * Retrieves the timeout before a TimeoutError is thrown when I/O
   * interest cannot be * fullfilled.
   */
  virtual Duration readTimeout() = 0;

  /**
   * Retrieves the timeout before a TimeoutError is thrown when I/O
   * interest cannot be * fullfilled.
   */
  virtual Duration writeTimeout() = 0;

  /**
   * Sets the timeout to wait for the read-interest before an TimeoutError is thrown.
   */
  virtual void setReadTimeout(Duration timeout) = 0;

  /**
   * Sets the timeout to wait for the read-interest before an TimeoutError is thrown.
   */
  virtual void setWriteTimeout(Duration timeout) = 0;

  /**
   * Tests whether this endpoint is blocking on I/O.
   */
  virtual bool isBlocking() const = 0;

  /**
   * Sets whether this endpoint is blocking on I/O or not.
   *
   * @param enable @c true to ensures I/O operations block (default), @c false
   *               otherwise.
   */
  virtual void setBlocking(bool enable) = 0;

  /**
   * Retrieves @c TCP_CORK state.
   */
  virtual bool isCorking() const = 0;

  /**
   * Sets whether to @c TCP_CORK or not.
   */
  virtual void setCorking(bool enable) = 0;

  virtual bool isTcpNoDelay() const = 0;

  virtual void setTcpNoDelay(bool enable) = 0;

  /**
   * String representation of the object for introspection.
   */
  virtual std::string toString() const = 0;

  virtual Option<InetAddress> remoteAddress() const;
  virtual Option<InetAddress> localAddress() const;

 private:
  std::unique_ptr<Connection> connection_;
};

template<typename T, typename... Args>
inline T* EndPoint::setConnection(Args&&... args) {
  setConnection(std::unique_ptr<T>(new T(std::forward<Args>(args)...)));
  return static_cast<T*>(connection());
}

}  // namespace xzero
