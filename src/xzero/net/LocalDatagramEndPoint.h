// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
