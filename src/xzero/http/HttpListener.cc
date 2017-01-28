// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpListener.h>

namespace xzero {
namespace http {

void HttpListener::onMessageBegin(const BufferRef& method,
                                  const BufferRef& uri,
                                  HttpVersion version) {
}

void HttpListener::onMessageBegin(HttpVersion version, HttpStatus code,
                                  const BufferRef& text) {
}

void HttpListener::onMessageBegin() {
}

}  // namespace http
}  // namespace xzero
