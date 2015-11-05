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
#include <xzero/Buffer.h>
#include <xzero/net/DatagramEndPoint.h>
#include <vector>

namespace xzero {

class LocalDatagramConnector;

/**
 * In-memory based datagram endpoint.
 *
 * @see LocalDatagramConnector.
 */
class XZERO_BASE_API LocalDatagramEndPoint : public DatagramEndPoint {
 public:
  LocalDatagramEndPoint(LocalDatagramConnector* connector, Buffer&& msg);
  ~LocalDatagramEndPoint();

  size_t send(const BufferRef& response) override;

  const std::vector<Buffer>& responses() const noexcept { return responses_; }

 private:
  std::vector<Buffer> responses_;
};

} // namespace xzero
