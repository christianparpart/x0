// This file is part of the "x0" project
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/HttpRequest.h>
#include <xzero/HttpServer.h>
#include <stdio.h>
#include <ev++.h>

int main() {
  xzero::HttpServer httpServer(ev::default_loop(0));
  httpServer.setupListener("0.0.0.0", 3000);

  printf("Serving HTTP from 0.0.0.0:3000 ...\n");

  httpServer.requestHandler = [](xzero::HttpRequest* r) {
    r->status = xzero::HttpStatus::Ok;
    r->write("Hello, HTTP World!\n");
    r->finish();
  };

  return httpServer.run();
}
