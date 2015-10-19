// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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
  logTrace("UdpEndPoint", "send(): %zu bytes", response.size());

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
