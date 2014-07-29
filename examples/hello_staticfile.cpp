// This file is part of the "x0" project
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/**
 * HTTP Server example for serving local static files.
 *
 * Highlights:
 *
 * This code has builtin support for:
 * <ul>
 *   <li> GET and HEAD requests </li>
 *   <li> client cache aware response handling </li>
 *   <li> Range requests </li>
 * </ul>
 *
 */

#include <xzero/HttpRequest.h>
#include <xzero/HttpServer.h>
#include <unistd.h>
#include <ev++.h>

int main() {
  xzero::HttpServer httpServer(ev::default_loop(0));
  httpServer.setupListener("0.0.0.0", 3000);
  httpServer.setLogLevel(base::Severity::info);

  char cwd[1024];
  if (!getcwd(cwd, sizeof(cwd))) {
    cwd[0] = '.';
    cwd[1] = '\0';
  }

  httpServer.log(base::Severity::info, "Serving HTTP from 0.0.0.0:3000 ...");

  httpServer.requestHandler = [&](xzero::HttpRequest* r) {
    r->log(base::Severity::info, "Serving: %s", r->path.str().c_str());
    r->documentRoot = cwd;
    r->fileinfo = r->connection.worker().fileinfo(r->documentRoot + r->path);
    r->sendfile(r->fileinfo);
    r->finish();
  };

  return httpServer.run();
}
