// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/client/HttpClient.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/net/ByteArrayEndPoint.h>
#include <xzero/Application.h>
#include <xzero/logging.h>
#include <xzero/stdtypes.h>
#include <gtest/gtest.h>

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
  Application::logToStderr(LogLevel::Trace);

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
