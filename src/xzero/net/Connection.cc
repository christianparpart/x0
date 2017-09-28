// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/StringUtil.h>
#include <xzero/net/Connection.h>
#include <xzero/net/TcpEndPoint.h>
#include <algorithm>

namespace xzero {

Connection::Connection(TcpEndPoint* endpoint,
                       Executor* executor)
    : endpoint_(endpoint),
      executor_(executor) {
}

Connection::~Connection() {
}

void Connection::onOpen(bool dataReady) {
}

void Connection::close() {
  if (endpoint_) {
    endpoint_->close();
  }
}

void Connection::wantRead() {
  endpoint()->wantRead();
}

void Connection::wantWrite() {
  endpoint()->wantWrite();
}

void Connection::onReadable() {
}

void Connection::onWriteable() {
}

void Connection::onInterestFailure(const std::exception& error) {
  close();
}

bool Connection::onReadTimeout() {
  // inform caller to close the endpoint
  return true;
}

}  // namespace xzero
