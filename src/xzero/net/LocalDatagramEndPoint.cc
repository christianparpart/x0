// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/net/LocalDatagramEndPoint.h>
#include <xzero/net/LocalDatagramConnector.h>

namespace xzero {

LocalDatagramEndPoint::LocalDatagramEndPoint(
    LocalDatagramConnector* connector, Buffer&& msg)
    : DatagramEndPoint(connector, std::move(msg)) {
}

LocalDatagramEndPoint::~LocalDatagramEndPoint() {
}

size_t LocalDatagramEndPoint::send(const BufferRef& response) {
  responses_.emplace_back(response);
  return response.size();
}

} // namespace xzero
