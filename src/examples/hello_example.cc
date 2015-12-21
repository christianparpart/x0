// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/Application.h>
#include <xzero/http/HttpService.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/executor/ThreadPool.h>

using namespace xzero;
using namespace xzero::http;

class HelloService : public HttpService::Handler {
 public:
  bool handleRequest(HttpRequest* request, HttpResponse* response) override;
};

bool HelloService::handleRequest(HttpRequest* request, HttpResponse* response) {
  // set response status code
  response->setStatus(HttpStatus::Ok);

  // set some custom response header
  response->addHeader("Hello", "World");

  // write some content to the client
  response->write("Hello, World!\n");

  // invoke completed() marking the response as being "complete",
  // i.e. fully generated.
  response->completed();

  // Tell the caller that we've handled the request,
  // otherwise another handler will be tried.
  return true;
}

int main(int argc, const char* argv[]) {
  Application::logToStderr(LogLevel::Trace);

  std::unique_ptr<ThreadPool> threadpool;
  PosixScheduler scheduler;
  HttpService service(HttpService::HTTP1);

  IPAddress bind = IPAddress("127.0.0.1");
  int port = 3000;

  Executor* clientExecutor = &scheduler;
  bool threaded = false; // TODO FIXME threaded mode (data races in http?)

  if (threaded) {
    threadpool.reset(new ThreadPool());
    clientExecutor = threadpool.get();
  }

  HelloService hello;
  service.addHandler(&hello);

  service.configureInet(&scheduler,
                        clientExecutor,
                        20_seconds, // read timeout
                        10_seconds, // write timeout
                        8_seconds, // TCP FIN timeout
                        bind,
                        port);

  service.start();

  scheduler.runLoop();

  return 0;
}
