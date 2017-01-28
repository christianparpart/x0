// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
