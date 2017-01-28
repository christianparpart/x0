// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/net/DatagramConnector.h>
#include <xzero/net/LocalDatagramEndPoint.h>
#include <xzero/net/IPAddress.h>
#include <functional>
#include <deque>

namespace xzero {

/**
 * Datagram Connector for in-memory messages.
 *
 * @see DatagramConnector, DatagramEndPoint
 * @see UdpConnector, UdpEndPoint
 */
class XZERO_BASE_API LocalDatagramConnector : public DatagramConnector {
 public:
  /**
   * Initializes the UDP connector.
   *
   * @param name Human readable name for the given connector (such as "ntp").
   * @param handler Callback handler to be invoked on every incoming message.
   * @param executor Executor service to be used for invoking the handler.
   */
  LocalDatagramConnector(
      const std::string& name,
      DatagramHandler handler,
      Executor* executor);

  ~LocalDatagramConnector();

  void start() override;
  bool isStarted() const override;
  void stop() override;

  RefPtr<LocalDatagramEndPoint> send(const BufferRef& message);
  RefPtr<LocalDatagramEndPoint> send(Buffer&& message);

 private:
  void runQueue();

 private:
  bool isStarted_;
  std::deque<RefPtr<LocalDatagramEndPoint>> pending_;
};

} // namespace xzero
