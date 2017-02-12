// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <initializer_list>
#include <vector>
#include <xzero/testing.h>
#include <xzero/http/fastcgi/RequestParser.h>
#include <xzero/http/fastcgi/ResponseParser.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpChannel.h>
#include <xzero/io/FileUtil.h>

using namespace xzero;
using namespace xzero::http;

/*
 * TODO:
 *
 * - BeginRequest: test flag keepAlive true/false properly exposed
 * - ...
 *
 */

class RequestListener : public HttpListener { // {{{
 public:
  void onMessageBegin(const BufferRef& method, const BufferRef& entity,
                      HttpVersion version) override;
  void onMessageBegin(HttpVersion version, HttpStatus code,
                      const BufferRef& text) override;
  void onMessageBegin() override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onProtocolError(HttpStatus code, const std::string& message) override;

 public:
  BufferRef method;
  BufferRef entity;
  HttpVersion version = HttpVersion::VERSION_0_9;
  HttpStatus status = HttpStatus::Undefined;
  BufferRef text;
  int requestMessageBeginCount = 0;
  int genericMessageBeginCount = 0;
  int responseMessageBeginCount = 0;
  HeaderFieldList headers;
  int headersEnd = 0;
  int messageEnd = 0;
  int protocolErrors = 0;
  Buffer body;
};

void RequestListener::onMessageBegin(const BufferRef& method, const BufferRef& entity,
                    HttpVersion version) {
  this->method = method;
  this->entity = entity;
  this->version = version;
  this->requestMessageBeginCount++;
}

void RequestListener::onMessageBegin(HttpVersion version, HttpStatus code,
                    const BufferRef& text) {
  this->version = version;
  this->status = code;
  this->text = text;
  this->responseMessageBeginCount++;
}

void RequestListener::onMessageBegin() {
  this->genericMessageBeginCount++;
}

void RequestListener::onMessageHeader(const BufferRef& name, const BufferRef& value) {
  this->headers.push_back(name.str(), value.str());
}

void RequestListener::onMessageHeaderEnd() {
  this->headersEnd++;
}

void RequestListener::onMessageContent(const BufferRef& chunk) {
  this->body.push_back(chunk);
}

void RequestListener::onMessageContent(FileView&& chunk) {
  this->body.push_back(FileUtil::read(chunk));
}

void RequestListener::onMessageEnd() {
  this->messageEnd++;
}

void RequestListener::onProtocolError(HttpStatus code, const std::string& message) {
  this->protocolErrors++;
}
// }}}

Buffer makeParams(const std::list<std::pair<std::string, std::string>>& params) {
  http::fastcgi::CgiParamStreamWriter w;
  for (const auto param: params)
    w.encode(param.first, param.second);

  Buffer result;
  result.reserve(2 * sizeof(http::fastcgi::Record) + w.output().size());

  // append Params record
  new(result.data()) http::fastcgi::Record(
      http::fastcgi::Type::Params, 42, w.output().size(), 0);
  result.resize(sizeof(http::fastcgi::Record));
  result.push_back(w.output());

  // append Params-EOS
  new(result.data() + result.size()) http::fastcgi::Record(
      http::fastcgi::Type::Params, 42, 0, 0);
  result.resize(result.size() + sizeof(http::fastcgi::Record));

  return result;
}

TEST(http_fastcgi_RequestParser, simpleRequest) {
  // Tests the following example from the FastCGI spec:
  //
  // {FCGI_BEGIN_REQUEST,   1, {FCGI_RESPONDER, 0}}
  // {FCGI_PARAMS,          1, "\013\002SERVER_PORT80\013\016SER"}
  // {FCGI_PARAMS,          1, "VER_ADDR199.170.183.42 ... "}
  // {FCGI_PARAMS,          1, ""}
  // {FCGI_STDIN,           1, "quantity=100&item=3047936"}
  // {FCGI_STDIN,           1, ""}

  int parsedRequestId = -1;
  std::vector<std::pair<std::string, std::string>> parsedHeaders;

  RequestListener httpListener;
  auto onCreateChannel = [&](int requestId, bool keepAlive) -> HttpListener* {
                            parsedRequestId = requestId;
                            return &httpListener; };
  auto onUnknownPacket = [&](int requestId, int recordId) { };
  auto onAbortRequest = [&](int requestId) { };

  http::fastcgi::RequestParser p(
      onCreateChannel, onUnknownPacket, onAbortRequest);

  size_t n = p.parseFragment<http::fastcgi::BeginRequestRecord>(
      http::fastcgi::Role::Responder, 42, false);
  EXPECT_EQ(sizeof(http::fastcgi::BeginRequestRecord), n);
  EXPECT_EQ(42, parsedRequestId);

  Buffer fcgistream = makeParams({
      {"SERVER_PORT", "80"},
      {"SERVER_PROTOCOL", "HTTP/1.1"},
      {"SERVER_NAME", "www.example.com"},
      {"REQUEST_METHOD", "GET"},
      {"REQUEST_URI", "/index.html"},
      {"HTTP_USER_AGENT", "xzero-test"},
      {"HTTP_CONTENT_TYPE", "text/plain"},
  });
  n = p.parseFragment(fcgistream);

  EXPECT_EQ(fcgistream.size(), n);
  EXPECT_EQ(0, httpListener.protocolErrors);

  // CONTENT
  constexpr BufferRef content = "quantity=100&item=3047936";
  fcgistream.reserve(sizeof(http::fastcgi::Record) + content.size());
  new(fcgistream.data()) http::fastcgi::Record(
      http::fastcgi::Type::StdIn, 42, content.size(), 0);
  fcgistream.resize(sizeof(http::fastcgi::Record));
  fcgistream.push_back(content);
  n = p.parseFragment(fcgistream);

  EXPECT_EQ(fcgistream.size(), n);
  EXPECT_EQ(0, httpListener.protocolErrors);
  EXPECT_EQ(content, httpListener.body);

  // content-EOS
  new(fcgistream.data()) http::fastcgi::Record(
      http::fastcgi::Type::StdIn, 42, 0, 0);
  fcgistream.resize(sizeof(http::fastcgi::Record));
  n = p.parseFragment(fcgistream);

  EXPECT_EQ(fcgistream.size(), n);

  EXPECT_EQ(1, httpListener.requestMessageBeginCount);
  EXPECT_EQ("GET", httpListener.method);
  EXPECT_EQ("/index.html", httpListener.entity);
  EXPECT_EQ(HttpVersion::VERSION_1_1, httpListener.version);

  EXPECT_EQ("xzero-test", httpListener.headers.get("User-Agent"));
  EXPECT_EQ("text/plain", httpListener.headers.get("Content-Type"));
  EXPECT_EQ(1, httpListener.headersEnd);

  EXPECT_EQ(1, httpListener.messageEnd);
  EXPECT_EQ(0, httpListener.protocolErrors);
}
