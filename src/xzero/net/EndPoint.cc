// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/EndPoint.h>
#include <xzero/net/Connection.h>
#include <xzero/Buffer.h>
#include <cassert>

namespace xzero {

EndPoint::EndPoint() noexcept
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

size_t EndPoint::fill(Buffer* sink) {
  int space = sink->capacity() - sink->size();
  if (space < 4 * 1024) {
    sink->reserve(sink->capacity() + 8 * 1024);
    space = sink->capacity() - sink->size();
  }

  return fill(sink, space);
}

}  // namespace xzero
