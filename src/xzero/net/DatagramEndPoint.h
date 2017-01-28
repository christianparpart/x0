// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
