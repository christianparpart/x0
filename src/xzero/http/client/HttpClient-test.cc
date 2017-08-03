// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/net/ByteArrayEndPoint.h>
#include <xzero/logging.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::client;

RefPtr<ByteArrayEndPoint> createEndPoint() {
  RefPtr<ByteArrayEndPoint> ep(new ByteArrayEndPoint());

  ep->setInput("HTTP/1.1 200 Ok\r\n"
               "Server: unittest\r\n"
               "Content-Length: 13\r\n"
               "\r\n"
               "Hello, World\n");

  return ep;
};

Future<RefPtr<EndPoint>> createEndPointAsync() {
  Promise<RefPtr<EndPoint>> promise;
  promise.success(createEndPoint().as<EndPoint>());
  return promise.future();
}

TEST(HttpClient, http1_default) {
  NativeScheduler sched(std::make_unique<CatchAndLogExceptionHandler>("unittest"));

  HttpClient cli(&sched, createEndPointAsync, 5_seconds);

  HttpRequest req(HttpVersion::VERSION_1_1, "GET", "/", {{"Host", "test"}}, false, {});

  auto f = cli.send(req);
  f.onSuccess([this](const HttpClient::Response& response) {
    log("onSuccess!");
  });
  f.onFailure([this](std::error_code) {
    log("onFailure!");
  });

  sched.runLoop();

  // TODO: find out why it is not actually handling the request / receiving response

  // EXPECT_EQ(200, (int) cli.responseInfo().status());
  // EXPECT_EQ("unittest", cli.responseInfo().headers().get("Server"));
  // EXPECT_EQ("Hello, World\n", cli.responseBody().str());
}
