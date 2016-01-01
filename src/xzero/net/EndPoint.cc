// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//   (c) 2014-2015 Paul Asmuth <https://github.com/paulasmuth>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/net/EndPoint.h>
#include <xzero/net/Connection.h>
#include <cassert>

namespace xzero {

EndPoint::EndPoint() XZERO_NOEXCEPT
    : connection_(nullptr) {
}

EndPoint::~EndPoint() {
}

void EndPoint::setConnection(std::unique_ptr<Connection>&& connection) {
  connection_ = std::move(connection);
}

Option<InetAddress> EndPoint::remoteAddress() const {
  return None();
}

Option<InetAddress> EndPoint::localAddress() const {
  return None();
}

}  // namespace xzero
