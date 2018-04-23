// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/StringUtil.h>
#include <xzero/net/TcpConnection.h>
#include <xzero/net/TcpEndPoint.h>
#include <algorithm>

namespace xzero {

TcpConnection::TcpConnection(TcpEndPoint* endpoint,
                             Executor* executor)
    : endpoint_(endpoint),
      executor_(executor) {
}

TcpConnection::~TcpConnection() {
}

void TcpConnection::onOpen(bool dataReady) {
}

void TcpConnection::close() {
  if (endpoint_) {
    endpoint_->close();
  }
}

void TcpConnection::wantRead() {
  endpoint()->wantRead();
}

void TcpConnection::wantWrite() {
  endpoint()->wantWrite();
}

void TcpConnection::onReadable() {
}

void TcpConnection::onWriteable() {
}

void TcpConnection::onInterestFailure(const std::exception& error) {
  close();
}

bool TcpConnection::onReadTimeout() {
  // inform caller to close the endpoint
  return true;
}

}  // namespace xzero
