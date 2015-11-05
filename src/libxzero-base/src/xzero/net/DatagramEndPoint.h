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
#include <xzero/RefCounted.h>
#include <functional>

namespace xzero {

class DatagramConnector;

class XZERO_BASE_API DatagramEndPoint : public RefCounted {
 public:
  DatagramEndPoint(DatagramConnector* connector, Buffer&& msg)
    : connector_(connector), message_(std::move(msg)) {}

  ~DatagramEndPoint();

  DatagramConnector* connector() const noexcept { return connector_; }

  const Buffer& message() const { return message_; }

  virtual size_t send(const BufferRef& response) = 0;

 private:
  //! the connector that was used to receive the message
  DatagramConnector* connector_;

  //! message received
  Buffer message_;
};

} // namespace xzero
