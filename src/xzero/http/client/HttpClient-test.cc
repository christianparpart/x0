// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClient.h>
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

TEST(HttpClient, DISABLED_test_http1_default) {
  NativeScheduler sched(std::unique_ptr<xzero::ExceptionHandler>(
      new CatchAndLogExceptionHandler("unittest")));

  RefPtr<ByteArrayEndPoint> ep = createEndPoint();
  HttpClient cli(&sched);

  HttpRequestInfo req(HttpVersion::VERSION_1_1, "GET", "/", 0, {});
  BufferRef body;

  cli.setRequest(std::move(req), body);
  cli.send(ep.as<EndPoint>());
  // TODO: find out why it is not actually handling the request / receiving response

  EXPECT_EQ(200, (int) cli.responseInfo().status());
  EXPECT_EQ("unittest", cli.responseInfo().headers().get("Server"));
  EXPECT_EQ("Hello, World\n", cli.responseBody().str());
}
