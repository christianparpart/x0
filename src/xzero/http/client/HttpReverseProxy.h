// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/net/InetAddress.h>
#include <xzero/http/HttpStatus.h>

namespace xzero {
namespace http {

class HttpRequest;
class HttpResponse;

namespace client {

class HttpReverseProxy {
 public:
  HttpReverseProxy(const InetAddress& upstream);
  ~HttpReverseProxy();

  /**
   * Serves given request to this reverse proxy.
   *
   * This call is thread safe.
   */
  HttpStatus serve(HttpRequest* request, HttpResponse* response);

 private:
  InetAddress upstreamAddress_;
};

} // namespace client
} // namespace http
} // namespace xzero
