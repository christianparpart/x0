// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/UdpEndPoint.h>
#include <xzero/net/UdpConnector.h>
#include <xzero/net/IPAddress.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

namespace xzero {

UdpEndPoint::UdpEndPoint(
    UdpConnector* connector,
    Buffer&& msg,
    struct sockaddr* remoteSock,
    int remoteSockLen)
    : DatagramEndPoint(connector, std::move(msg)),
      remoteSock_((char*) remoteSock, remoteSockLen) {
}

UdpEndPoint::~UdpEndPoint() {
  //static_cast<UdpConnector*>(connector())->release(this);
}

size_t UdpEndPoint::send(const BufferRef& response) {
#ifndef NDEBUG
  logTrace("UdpEndPoint", "send(): $0 bytes", response.size());
#endif

  const int flags = 0;
  ssize_t n;

  do {
    n = sendto(
        static_cast<UdpConnector*>(connector())->handle(),
        response.data(),
        response.size(),
        flags,
        (struct sockaddr*) remoteSock_.data(),
        remoteSock_.size());
  } while (n < 0 && errno == EINTR);

  if (n < 0)
    RAISE_ERRNO(errno);

  return n;
}


} // namespace xzero
