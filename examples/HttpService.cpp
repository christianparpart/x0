// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/*! \brief Very simple HTTP server. Everything's done for you.
 *
 * It just serves static pages.
 */

#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <base/io/BufferSource.h>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <ev++.h>

class MyHttpService {
 private:
  struct ev_loop* loop_;
  ev::sig sigterm_;
  ev::sig sigint_;
  std::unique_ptr<xzero::HttpServer> http_;

 public:
  MyHttpService()
      : loop_(ev_default_loop()), sigterm_(loop_), sigint_(loop_), http_() {
    std::clog << "Initializing ..." << std::endl;

    sigterm_.set<MyHttpService, &MyHttpService::terminateHandler>(this);
    sigterm_.start(SIGTERM);
    ev_unref(loop_);

    sigint_.set<MyHttpService, &MyHttpService::terminateHandler>(this);
    sigint_.start(SIGINT);
    ev_unref(loop_);

    http_.reset(new xzero::HttpServer(loop_));
    http_->requestHandler =
        std::bind(&MyHttpService::requestHandler, this, std::placeholders::_1);
    http_->setupListener("0.0.0.0", 3000);

    // run HTTP with 4 worker threads (1 main thread + 3 posix threads)
    while (http_->workers().size() < 4) {
      http_->createWorker();
    }
  }

  ~MyHttpService() {
    std::clog << "Quitting ..." << std::endl;

    if (sigterm_.is_active()) {
      ev_ref(loop_);
      sigterm_.stop();
    }

    if (sigint_.is_active()) {
      ev_ref(loop_);
      sigint_.stop();
    }
  }

  int run() {
    std::clog << "Listening on http://0.0.0.0:3000 ..." << std::endl;
    return http_->run();
  }

 private:
  bool requestHandler(xzero::HttpRequest* r) {
    if (r->method != "HEAD" && r->method != "GET") {
      r->status = xzero::HttpStatus::MethodNotAllowed;
      r->responseHeaders.push_back("Allow", "GET, HEAD");
      r->finish();
      return true;
    }

    base::Buffer body;
    body.push_back("Hello, World!\n");

    r->status = xzero::HttpStatus::Ok;
    r->responseHeaders.push_back("Content-Type", "text/plain");
    r->responseHeaders.push_back("Content-Length", std::to_string(body.size()));
    r->responseHeaders.push_back("X-Worker-ID",
                                 std::to_string(http_->currentWorker()->id()));

    if (r->method != "HEAD") {
      r->write<base::BufferSource>(std::move(body));
    }

    r->finish();

    return true;
  }

  void terminateHandler(ev::sig& sig, int) {
    std::clog << "Signal (" << sig.signum << ") received. Terminating."
              << std::endl;

    ev_ref(loop_);
    sig.stop();

    http_->stop();
  }
};

int main(int argc, const char* argv[]) {
  MyHttpService svc;
  return svc.run();
}
