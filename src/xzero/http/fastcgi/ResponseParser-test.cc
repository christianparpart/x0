// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
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

class ResponseListener : public HttpListener { // {{{
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
  void onError(std::error_code ec) override;

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

void ResponseListener::onMessageBegin(const BufferRef& method, const BufferRef& entity,
                    HttpVersion version) {
  this->method = method;
  this->entity = entity;
  this->version = version;
  this->requestMessageBeginCount++;
}

void ResponseListener::onMessageBegin(HttpVersion version, HttpStatus code,
                    const BufferRef& text) {
  this->version = version;
  this->status = code;
  this->text = text;
  this->responseMessageBeginCount++;
}

void ResponseListener::onMessageBegin() {
  this->genericMessageBeginCount++;
}

void ResponseListener::onMessageHeader(const BufferRef& name, const BufferRef& value) {
  this->headers.push_back(name.str(), value.str());
}

void ResponseListener::onMessageHeaderEnd() {
  this->headersEnd++;
}

void ResponseListener::onMessageContent(const BufferRef& chunk) {
  this->body.push_back(chunk);
}

void ResponseListener::onMessageContent(FileView&& chunk) {
  this->body.push_back(FileUtil::read(chunk));
}

void ResponseListener::onMessageEnd() {
  this->messageEnd++;
}

void ResponseListener::onError(std::error_code ec) {
  this->protocolErrors++;
}
// }}}

TEST(http_fastcgi_ResponseParser, simpleResponse) {
  // Tests the following example from the FastCGI spec:
  //
  // {FCGI_STDOUT,      1, "Content-type: text/html\r\n\r\n<html>\n<head> ... "}
  // {FCGI_STDOUT,      1, ""}
  // {FCGI_END_REQUEST, 1, {0, FCGI_REQUEST_COMPLETE}}

  // --------------------------------------------------------------------------
  // generate response bitstream

  constexpr BufferRef content =
      "Status: 200\r\n"
      "Content-Type: text/html\r\n"
      "\r\n"
      "<html>\n<head> ...";

  Buffer fcgistream;
  fcgistream.reserve(1024);

  // StdOut-header with payload
  new(fcgistream.data()) http::fastcgi::Record(
      http::fastcgi::Type::StdOut, 1, content.size(), 0);
  fcgistream.resize(sizeof(http::fastcgi::Record));
  fcgistream.push_back(content);

  // StdOut-header (EOS)
  new(fcgistream.end()) http::fastcgi::Record(
      http::fastcgi::Type::StdOut, 1, 0, 0);
  fcgistream.resize(fcgistream.size() + sizeof(http::fastcgi::Record));

  // EndRequest-header
  new(fcgistream.end()) http::fastcgi::EndRequestRecord(
      1, // requestId
      0, // appStatus
      http::fastcgi::ProtocolStatus::RequestComplete);
  fcgistream.resize(fcgistream.size() + sizeof(http::fastcgi::EndRequestRecord));

  // --------------------------------------------------------------------------
  // parse the response bitstream

  int parsedRequestId = -1;
  ResponseListener httpListener;
  auto onCreateChannel = [&](int requestId) -> HttpListener* {
                            parsedRequestId = requestId;
                            return &httpListener; };
  auto onUnknownPacket = [&](int requestId, int recordId) { };
  auto onStdErr = [&](int requestId, const BufferRef& content) { };

  http::fastcgi::ResponseParser parser(
      onCreateChannel, onUnknownPacket, onStdErr);

  size_t n = parser.parseFragment(fcgistream);

  EXPECT_EQ(fcgistream.size(), n);
  EXPECT_EQ(0, httpListener.protocolErrors);
  EXPECT_EQ(1, parsedRequestId);
  EXPECT_EQ("text/html", httpListener.headers.get("Content-Type"));
  EXPECT_EQ("<html>\n<head> ...", httpListener.body);
}
