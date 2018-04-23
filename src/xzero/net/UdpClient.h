// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/Buffer.h>
#include <xzero/net/InetAddress.h>
#include <xzero/net/Socket.h>

namespace xzero {

/**
 * Simple UDP client.
 */
class UdpClient {
 public:
  explicit UdpClient(const InetAddress& inet);
  ~UdpClient();

  /**
   * Receives the underlying system handle for given UDP communication.
   *
   * Use this for Event registration in Scheduler API.
   */
  const Socket& handle() const noexcept { return socket_; }

  size_t send(const BufferRef& message);
  size_t receive(Buffer* message);

 private:
  Socket socket_;
  void* sockAddr_;
  int sockAddrLen_;
};

} // namespace xzero
