// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

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

