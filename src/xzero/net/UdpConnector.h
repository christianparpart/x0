// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/executor/Executor.h>
#include <xzero/net/IPAddress.h>
#include <xzero/RefPtr.h>
#include <functional>

namespace xzero {

class UdpEndPoint;

/**
 * Datagram Connector for UDP protocol.
 *
 * @see UdpEndPoint
 */
class UdpConnector {
 public:
  typedef std::function<void(RefPtr<UdpEndPoint>)> Handler;

  /**
   * Initializes the UDP connector.
   *
   * @param name Human readable name for the given connector (such as "ntp").
   * @param handler Callback handler to be invoked on every incoming message.
   * @param executor Executor service to be used for invoking the handler.
   * @param ipaddress IP address to bind the connector to.
   * @param port UDP port number to bind the connector to.
   * @param reuseAddr Whether or not to enable @c SO_REUSEADDR.
   * @param reusePort Whether or not to enable @c SO_REUSEPORT.
   */
  UdpConnector(const std::string& name,
               Handler handler,
               Executor* executor,
               const IPAddress& ipaddress, int port,
               bool reuseAddr, bool reusePort);

  ~UdpConnector();

  Handler handler() const;

  int handle() const noexcept { return socket_; }

  /**
   * Starts handling incoming messages.
   */
  void start();

  /**
   * Whether or not incoming messages are being handled.
   */
  bool isStarted() const;

  /**
   * Stops handling incoming messages.
   */
  void stop();

  Executor* executor() const noexcept;

 private:
  void open(const IPAddress& bind, int port, bool reuseAddr, bool reusePort);

  void notifyOnEvent();
  void onMessage();

 private:
  std::string name_;
  Handler handler_;
  Executor* executor_;

  Executor::HandleRef io_;
  int socket_;
  int addressFamily_;
};

inline Executor* UdpConnector::executor() const noexcept {
  return executor_;
}

} // namespace xzero
