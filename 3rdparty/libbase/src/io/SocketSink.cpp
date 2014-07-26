// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <base/io/SocketSink.h>
#include <base/io/SinkVisitor.h>
#include <base/Socket.h>

namespace base {

SocketSink::SocketSink(Socket *s) : socket_(s) {}

void SocketSink::accept(SinkVisitor &v) { v.visit(*this); }

ssize_t SocketSink::write(const void *buffer, size_t size) {
  return socket_->write(buffer, size);
}

}  // namespace base
