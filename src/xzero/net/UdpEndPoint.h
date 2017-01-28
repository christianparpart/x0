// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/Api.h>
#include <xzero/net/DatagramEndPoint.h>
#include <xzero/net/IPAddress.h>
#include <xzero/Buffer.h>

namespace xzero {

class UdpConnector;
class IPAddress;

class XZERO_BASE_API UdpEndPoint : public DatagramEndPoint {
 public:
  UdpEndPoint(
      UdpConnector* connector, Buffer&& msg,
      struct sockaddr* remoteSock, int remoteSockLen);
  ~UdpEndPoint();

  size_t send(const BufferRef& response) override;

 private:
  Buffer remoteSock_;
};

} // namespace xzero

