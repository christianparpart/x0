// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <memory>
#include <list>

namespace xzero {

class Executor;
class Connector;
class EndPoint;

/**
 * A Connection is responsible for processing an EndPoint.
 *
 * A Connection derivate can implement various stream oriented protocols.
 * This doesn't necessarily has to be HTTP, but can also be SMTP or anything
 * else.
 */
class XZERO_BASE_API Connection {
 public:
  Connection(EndPoint* endpoint, Executor* executor);
  virtual ~Connection();

  /**
   * Callback, invoked when connection was opened.
   *
   * @param dataReady true if there is already data available
   *                  for read without blocking, false otherwise.
   */
  virtual void onOpen(bool dataReady);

  /**
   * Retrieves the corresponding endpoint for this connection.
   */
  EndPoint* endpoint() const XZERO_NOEXCEPT;

  /**
   * Retrieves the Executor that may be used for handling this connection.
   */
  Executor* executor() const XZERO_NOEXCEPT;

  /**
   * Closes the underlying endpoint.
   */
  virtual void close();

  /**
   * Ensures onFillable() is invoked when data is available for
   * read.
   *
   * In any case of an error, onInterestFailure(const std::exception&) is invoked.
   */
  void wantFill();

  /**
   * Ensures onFlushable() is invoked when underlying endpoint is ready
   * to write.
   *
   * In any case of an error, onInterestFailure(const std::exception&) is invoked.
   */
  void wantFlush();

  /**
   * Event callback being invoked when data is available for read.
   */
  virtual void onFillable();

  /**
   * Event callback being invoked when underlying endpoint ready for write.
   */
  virtual void onFlushable();

  /**
   * Event callback being invoked on any errors while waiting for data.
   *
   * For example read timeout (or possibly connection timeout?).
   *
   * The default implementation simply invokes abort().
   */
  virtual void onInterestFailure(const std::exception& error);

  /**
   * Callback being invoked when a read-timeout has been reached.
   *
   * @retval true close the endpoint.
   * @retval false ignore the timeout, do not close.
   *
   * By default the implementation of Connection::onReadTimeout() returns just
   * true.
   */
  virtual bool onReadTimeout();

 private:
  EndPoint* endpoint_;
  Executor* executor_;
};

inline EndPoint* Connection::endpoint() const XZERO_NOEXCEPT {
  return endpoint_;
}

inline Executor* Connection::executor() const XZERO_NOEXCEPT {
  return executor_;
}

} // namespace xzero
