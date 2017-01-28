// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

// HTTP semantic tests

#include <xzero/http/mock/Transport.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/BadMessage.h>

#include <xzero/executor/LocalExecutor.h>
#include <xzero/Buffer.h>

#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;

void handlerOk(HttpRequest* request, HttpResponse* response) {
  response->setStatus(HttpStatus::Ok);
  response->completed();
}

void sendError504(HttpRequest* request, HttpResponse* response) {
  response->sendError(HttpStatus::GatewayTimeout);
}

TEST(http_HttpChannel, sameVersion) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &handlerOk);
  transport.run(HttpVersion::VERSION_1_0, "GET", "/", {}, "");
  ASSERT_EQ(HttpVersion::VERSION_1_0, transport.responseInfo().version());
}

TEST(http_HttpChannel, sendError504) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &sendError504);
  transport.run(HttpVersion::VERSION_1_0, "GET", "/", {}, "");
  ASSERT_EQ((int)HttpStatus::GatewayTimeout, (int)transport.responseInfo().status());
  ASSERT_TRUE(transport.responseInfo().contentLength() == transport.responseBody().size());
}

TEST(http_HttpChannel, invalidRequestPath_escapeDocumentRoot1) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &handlerOk);
  transport.run(HttpVersion::VERSION_1_0, "GET", "/../../etc/passwd", {}, "");
  ASSERT_EQ(HttpStatus::BadRequest, transport.responseInfo().status());
}

TEST(http_HttpChannel, invalidRequestPath_escapeDocumentRoot2) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &handlerOk);
  transport.run(HttpVersion::VERSION_1_0, "GET", "/..\%2f..\%2fetc/passwd", {}, "");
  ASSERT_EQ(HttpStatus::BadRequest, transport.responseInfo().status());
}

TEST(http_HttpChannel, invalidRequestPath_injectNullByte1) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &handlerOk);
  transport.run(HttpVersion::VERSION_1_0, "GET", "/foo%00", {}, "");
  ASSERT_EQ(HttpStatus::BadRequest, transport.responseInfo().status());
}

TEST(http_HttpChannel, invalidRequestPath_injectNullByte2) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &handlerOk);
  transport.run(HttpVersion::VERSION_1_0, "GET", "/foo%00/bar", {}, "");
  ASSERT_EQ(HttpStatus::BadRequest, transport.responseInfo().status());
}

TEST(http_HttpChannel, missingHostHeader) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &handlerOk);
  transport.run(HttpVersion::VERSION_1_1, "GET", "/", {}, "");
  ASSERT_EQ(HttpStatus::BadRequest, transport.responseInfo().status());

  transport.run(HttpVersion::VERSION_1_0, "GET", "/", {}, "");
  ASSERT_EQ(HttpStatus::Ok, transport.responseInfo().status());

  transport.run(HttpVersion::VERSION_0_9, "GET", "/", {}, "");
  ASSERT_EQ(HttpStatus::Ok, transport.responseInfo().status());
}

TEST(http_HttpChannel, multipleHostHeaders) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &handlerOk);
  transport.run(HttpVersion::VERSION_1_1, "GET", "/", {
      {"Host", "foo"}, {"Host", "bar"}}, "");
  ASSERT_EQ((int)HttpStatus::BadRequest, (int)transport.responseInfo().status());
}

TEST(http_HttpChannel, unhandledException1) {
  LocalExecutor executor;
  mock::Transport transport(&executor, [](HttpRequest*, HttpResponse*) {
    throw std::runtime_error("me, the unhandled");
  });
  transport.run(HttpVersion::VERSION_1_0, "GET", "/", {}, "");
  ASSERT_EQ(HttpStatus::InternalServerError, transport.responseInfo().status());
  ASSERT_EQ("me, the unhandled", transport.responseInfo().reason());
}

TEST(http_HttpChannel, unhandledException2) {
  LocalExecutor executor;
  mock::Transport transport(&executor, [](HttpRequest*, HttpResponse*) {
    throw 42;
  });
  transport.run(HttpVersion::VERSION_1_0, "GET", "/", {}, "");
  ASSERT_EQ(HttpStatus::InternalServerError, transport.responseInfo().status());
}

TEST(http_HttpChannel, completed_invoked_before_contentLength_satisfied) {
  LocalExecutor executor;
  mock::Transport transport(&executor, [](HttpRequest* request,
                                          HttpResponse* response) {
    response->setStatus(HttpStatus::Ok);
    response->addHeader("Content-Type", "text/plain");
    response->setContentLength(10);
    response->write("12345");
    response->completed();
  });

  // we expect the connection to terminate on invalid generated
  // response messages

  EXPECT_THROW(transport.run(HttpVersion::VERSION_1_1, "GET", "/", {{"Host", "test"}}, ""), RuntimeError);
  //EXPECT_TRUE(transport.isAborted());
}

TEST(http_HttpChannel, trailer1) {
  LocalExecutor executor;
  mock::Transport transport(&executor, [](HttpRequest* request,
                                          HttpResponse* response) {
    response->setStatus(HttpStatus::Ok);
    response->addHeader("Content-Type", "text/plain");
    response->registerTrailer("Word-Count");
    response->registerTrailer("Mood");
    response->write("Hello, World!\n");
    response->setTrailer("Word-Count", "one");
    response->setTrailer("Mood", "Happy");
    response->completed();
  });

  transport.run(HttpVersion::VERSION_1_1, "GET", "/", {{"Host", "blah"}}, "");

  ASSERT_EQ((int)HttpStatus::Ok, (int)transport.responseInfo().status());
  ASSERT_EQ(2, transport.responseInfo().trailers().size());
  ASSERT_EQ("one", transport.responseInfo().trailers().get("Word-Count"));
  ASSERT_EQ("Happy", transport.responseInfo().trailers().get("Mood"));
}
