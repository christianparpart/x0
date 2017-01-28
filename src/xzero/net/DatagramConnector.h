// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/RefPtr.h>
#include <functional>
#include <string>

namespace xzero {

class Executor;
class DatagramEndPoint;

typedef std::function<void(RefPtr<DatagramEndPoint>)> DatagramHandler;

/**
 * Datagram Connector.
 *
 * @see DatagramConnector, DatagramEndPoint
 */
class XZERO_BASE_API DatagramConnector {
 public:
  /**
   * Initializes the UDP connector.
   *
   * @param name Human readable name for the given connector (such as "ntp").
   * @param handler Callback handler to be invoked on every incoming message.
   * @param executor Executor service to be used for invoking the handler.
   */
  DatagramConnector(
      const std::string& name,
      DatagramHandler handler,
      Executor* executor);

  virtual ~DatagramConnector();

  DatagramHandler handler() const;
  void setHandler(DatagramHandler handler);

  /**
   * Starts handling incoming messages.
   */
  virtual void start() = 0;

  /**
   * Whether or not incoming messages are being handled.
   */
  virtual bool isStarted() const = 0;

  /**
   * Stops handling incoming messages.
   */
  virtual void stop() = 0;

  Executor* executor() const noexcept;

 protected:
  std::string name_;
  DatagramHandler handler_;
  Executor* executor_;
};

inline Executor* DatagramConnector::executor() const noexcept {
  return executor_;
}

} // namespace xzero
