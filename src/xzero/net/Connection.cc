// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/StringUtil.h>
#include <xzero/net/Connection.h>
#include <xzero/net/EndPoint.h>
#include <algorithm>

namespace xzero {

Connection::Connection(EndPoint* endpoint,
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

void Connection::wantFill() {
  endpoint()->wantFill();
}

void Connection::wantFlush() {
  endpoint()->wantFlush();
}

void Connection::onFillable() {
}

void Connection::onFlushable() {
}

void Connection::onInterestFailure(const std::exception& error) {
  close();
}

bool Connection::onReadTimeout() {
  // inform caller to close the endpoint
  return true;
}

template<>
std::string StringUtil::toString(Connection* c) {
  return StringUtil::format("Connection[$0]", c->endpoint()->remoteAddress());
}

}  // namespace xzero
