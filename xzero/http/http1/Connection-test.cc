// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

// HTTP/1 transport protocol tests

#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/executor/LocalExecutor.h>
#include <xzero/logging/LogTarget.h>
#include <xzero/net/Server.h>
#include <xzero/net/LocalConnector.h>
#include <xzero/Buffer.h>
#include <xzero/testing.h>

#include <xzero/http/http1/Parser.h>
#include <xzero/http/HttpListener.h>
#include <xzero/HugeBuffer.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::http1;

// FIXME HTTP/1.1 with keep-alive) SEGV's on LocalEndPoint.
//
// TODO test that userapp cannot add invalid headers
//      (e.g. connection level headers, such as Connection, TE,
//      Transfer-Encoding, Keep-Alive)
//      - this is actually a semantic check,
//        http1::Channel should prohibit such things

class ResponseParser : public HttpListener { // {{{
 public:
  ResponseParser();
  size_t parse(const BufferRef& response);

  const HttpResponseInfo& responseInfo() const { return responseInfo_; };
  const HugeBuffer& responseBody() const { return responseBody_; }

 private:
  void onMessageBegin(HttpVersion version, HttpStatus code,
                      const BufferRef& text) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onProtocolError(HttpStatus code, const std::string& message) override;

 private:
  HttpResponseInfo responseInfo_;
  HugeBuffer responseBody_;
};

ResponseParser::ResponseParser()
  : responseInfo_(),
    responseBody_(1024) {
}

size_t ResponseParser::parse(const BufferRef& response) {
  http1::Parser parser(Parser::RESPONSE, this);
  responseInfo_.reset();
  responseBody_.reset();
  return parser.parseFragment(response);
}

void ResponseParser::onMessageBegin(HttpVersion version, HttpStatus code,
                    const BufferRef& text) {
  responseInfo_.setVersion(version);
  responseInfo_.setStatus(code);
  responseInfo_.setReason(text.str());
}

void ResponseParser::onMessageHeader(const BufferRef& name, const BufferRef& value) {
  responseInfo_.headers().push_back(name.str(), value.str());
}

void ResponseParser::onMessageHeaderEnd() {
}

void ResponseParser::onMessageContent(const BufferRef& chunk) {
  responseBody_.write(chunk);
}

void ResponseParser::onMessageContent(FileView&& chunk) {
  responseBody_.write(std::move(chunk));
}

void ResponseParser::onMessageEnd() {
  responseInfo_.setContentLength(responseBody_.size());
  // promise_->success(this);
}

void ResponseParser::onProtocolError(HttpStatus code, const std::string& message) {
  // TODO promise_->failure(Status::ForeignError);
}
// }}}
class ScopedLogger { // {{{
 public:
  ScopedLogger() {
    // xzero::LogAggregator::get().setLogLevel(xzero::LogLevel::Trace);
    // xzero::LogAggregator::get().setLogTarget(xzero::LogTarget::console());
  }
  ~ScopedLogger() {
    // xzero::LogAggregator::get().setLogLevel(xzero::LogLevel::None);
    // xzero::LogAggregator::get().setLogTarget(nullptr);
  }
};
// }}}

static const size_t requestHeaderBufferSize = 8 * 1024;
static const size_t requestBodyBufferSize = 8 * 1024;
static const size_t maxRequestUriLength = 64;
static const size_t maxRequestBodyLength = 128;
static const size_t maxRequestCount = 5;
static const Duration maxKeepAlive = 30_seconds;

#define SCOPED_LOGGER() ScopedLogger _scoped_logger_;
#define MOCK_HTTP1_SERVER(server, localConnector, executor)                     \
  xzero::Server server;                                                        \
  xzero::LocalExecutor executor(false);                                       \
  auto localConnector = server.addConnector<xzero::LocalConnector>(&executor); \
  auto http = std::make_unique<xzero::http::http1::ConnectionFactory>(         \
      requestHeaderBufferSize, requestBodyBufferSize,                          \
      maxRequestUriLength, maxRequestBodyLength, maxRequestCount,              \
      maxKeepAlive, false, false);                                             \
  http->setHandler([&](HttpRequest* request, HttpResponse* response) {         \
      response->setStatus(HttpStatus::Ok);                                     \
      response->setContentLength(request->path().size() + 1);                  \
      response->setHeader("Content-Type", "text/plain");                       \
      response->write(Buffer(request->path() + "\n"),                          \
          std::bind(&HttpResponse::completed, response));                       \
  });                                                                           \
  localConnector->addConnectionFactory(http->protocolName(),                  \
      std::bind(&HttpConnectionFactory::create, http.get(),                   \
                std::placeholders::_1,                                        \
                std::placeholders::_2));                                      \
  server.start();

TEST(http_http1_Connection, ConnectionClose_1_1) {
  MOCK_HTTP1_SERVER(server, connector, executor);

  xzero::RefPtr<LocalEndPoint> ep;
  executor.execute([&] {
    ep = connector->createClient("GET / HTTP/1.1\r\n"
                                 "Host: test\r\n"
                                 "Connection: close\r\n"
                                 "\r\n");
  });

  Buffer output = ep->output();
  ResponseParser resp;
  resp.parse(output);
  EXPECT_EQ(HttpVersion::VERSION_1_1, resp.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, resp.responseInfo().status());
  EXPECT_EQ("close", resp.responseInfo().getHeader("Connection"));
}

TEST(http_http1_Connection, ConnectionClose_1_0) {
  MOCK_HTTP1_SERVER(server, connector, executor);

  xzero::RefPtr<LocalEndPoint> ep;
  executor.execute([&] {
    ep = connector->createClient("GET / HTTP/1.0\r\n"
                                 "\r\n");
  });

  Buffer output = ep->output();
  ResponseParser resp;
  resp.parse(output);
  EXPECT_EQ(HttpVersion::VERSION_1_0, resp.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, resp.responseInfo().status());
  EXPECT_EQ("close", resp.responseInfo().getHeader("Connection"));
}

// sends one single request
TEST(http_http1_Connection, ConnectionKeepAlive_1_0) {
  MOCK_HTTP1_SERVER(server, connector, executor);

  xzero::RefPtr<LocalEndPoint> ep;
  executor.execute([&] {
    ep = connector->createClient("GET /hello HTTP/1.0\r\n"
                                 "Connection: Keep-Alive\r\n"
                                 "\r\n");
  });

  Buffer output = ep->output();
  ResponseParser resp;
  size_t n = resp.parse(output);
  EXPECT_EQ(HttpVersion::VERSION_1_0, resp.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, resp.responseInfo().status());
  EXPECT_EQ("Keep-Alive", resp.responseInfo().getHeader("Connection"));
  EXPECT_EQ("/hello\n", resp.responseBody().getBuffer());
}

// sends one single request
TEST(http_http1_Connection, ConnectionKeepAlive_1_1) {
  MOCK_HTTP1_SERVER(server, connector, executor);

  xzero::RefPtr<LocalEndPoint> ep;
  executor.execute([&] {
    ep = connector->createClient("GET /hello HTTP/1.1\r\n"
                                 "Host: test\r\n"
                                 "\r\n");
    //printf("%s\n", ep->output().str().c_str());
  });

  Buffer output = ep->output();
  ResponseParser resp;
  size_t n = resp.parse(output);
  EXPECT_EQ(HttpVersion::VERSION_1_1, resp.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, resp.responseInfo().status());
  EXPECT_EQ("Keep-Alive", resp.responseInfo().getHeader("Connection"));
  EXPECT_EQ("/hello\n", resp.responseBody().getBuffer());
}

// sends single request, gets response, sends another one on the same line.
// TEST(http_http1_Connection, ConnectionKeepAlive2) { TODO
// }

// sends 3 requests pipelined all at once. receives responses in order
TEST(http_http1_Connection, ConnectionKeepAlive3_pipelined) {
  //SCOPED_LOGGER();
  MOCK_HTTP1_SERVER(server, connector, executor);
  xzero::RefPtr<LocalEndPoint> ep;
  executor.execute([&] {
    ep = connector->createClient("GET /one HTTP/1.1\r\nHost: test\r\n\r\n"
                                 "GET /two HTTP/1.1\r\nHost: test\r\n\r\n"
                                 "GET /three HTTP/1.1\r\nHost: test\r\n\r\n");
  });

  Buffer output = ep->output();

  ResponseParser resp;
  size_t n = resp.parse(output);
  EXPECT_EQ(HttpVersion::VERSION_1_1, resp.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, resp.responseInfo().status());
  EXPECT_EQ("Keep-Alive", resp.responseInfo().getHeader("Connection"));
  EXPECT_EQ("/one\n", resp.responseBody().getBuffer());

  n += resp.parse(output.ref(n));
  EXPECT_EQ(HttpVersion::VERSION_1_1, resp.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, resp.responseInfo().status());
  EXPECT_EQ("Keep-Alive", resp.responseInfo().getHeader("Connection"));
  EXPECT_EQ("/two\n", resp.responseBody().getBuffer());

  n += resp.parse(output.ref(n));
  EXPECT_EQ(HttpVersion::VERSION_1_1, resp.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, resp.responseInfo().status());
  EXPECT_EQ("Keep-Alive", resp.responseInfo().getHeader("Connection"));
  EXPECT_EQ("/three\n", resp.responseBody().getBuffer());

  // no garbage should have been generated at the end
  ASSERT_EQ(n, output.size());
}

// ensure proper error code on bad request line
TEST(http_http1_Connection, protocolErrorShouldRaise400) {
  // SCOPED_LOGGER();
  MOCK_HTTP1_SERVER(server, connector, executor);
  xzero::RefPtr<LocalEndPoint> ep;
  executor.execute([&] {
    // FIXME HTTP/1.1 (due to keep-alive) SEGV's on LocalEndPoint.
    ep = connector->createClient("GET\r\n\r\n");
  });

  Buffer output = ep->output();
  ResponseParser resp;
  resp.parse(output);
  EXPECT_EQ(HttpVersion::VERSION_0_9, resp.responseInfo().version());
  EXPECT_EQ(HttpStatus::BadRequest, resp.responseInfo().status());
}
