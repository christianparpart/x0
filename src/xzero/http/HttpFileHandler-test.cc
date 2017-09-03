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
#include <xzero/http/HttpStatus.h>
#include <xzero/http/HttpFileHandler.h>
#include <xzero/io/MemoryFileRepository.h>
#include <xzero/executor/LocalExecutor.h>
#include <xzero/io/FileUtil.h>
#include <xzero/MimeTypes.h>
#include <xzero/Buffer.h>
#include <xzero/CivilTime.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;

class http_HttpFileHandler : public testing::Test { // {{{
 private:
  MimeTypes mimetypes_;
  UnixTime mtime_;
  MemoryFileRepository vfs_;
  HttpFileHandler staticFileHandler_;

 public:
  http_HttpFileHandler();
  static std::string generateBoundaryID();
  void staticfileHandler(HttpRequest* request, HttpResponse* response);
};

std::string http_HttpFileHandler::generateBoundaryID() {
  return "HelloBoundaryID";
}

http_HttpFileHandler::http_HttpFileHandler()
  : mimetypes_(),
    mtime_(CivilTime(2016, 8, 17, 3, 26, 0, 0, 0)),
    vfs_(mimetypes_),
    staticFileHandler_(&generateBoundaryID) {

  vfs_.insert("/12345.txt", mtime_, "12345");
  vfs_.insert("/fail-perm", mtime_, EPERM);
  vfs_.insert("/fail-access", mtime_, EACCES);
}

void http_HttpFileHandler::staticfileHandler(HttpRequest* request, HttpResponse* response) {
  auto file = vfs_.getFile(request->path());

  HttpStatus status = staticFileHandler_.handle(request, response, file);
  if (!isError(status))
    return;

  response->setStatus(status);
  response->completed();
}
// }}}

/*
 * [x] 200, basic GET
 * [x] 404, file not found
 * [x] 403, permission failure (EPERM, EACCESS)
 * [ ] (conditional request) If-None-Match
 * [ ] (conditional request) If-Match
 * [ ] (conditional request) If-Modified-Since
 * [ ] (conditional request) If-Unmodified-Since
 * [x] (ranged request) full range
 * [ ] (ranged request) invalid range (for example "0-4" instead of "range=0-4")
 * [x] (ranged request) first N bytes
 * [x] (ranged request) last N bytes
 * [x] (ranged request) multiple ranges
 * [x] HEAD on basic file
 * [ ] HEAD on conditional request
 * [ ] HEAD on ranged request
 * [ ] non-GET/HEAD (should result in MethodNotAllowed)
 * [ ] ensure we checked for 412 (Precondition Failed)
 */

TEST_F(http_HttpFileHandler, GET_FileNotFound) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/notfound.txt",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(404, static_cast<int>(transport.responseInfo().status()));
}

TEST_F(http_HttpFileHandler, GET_Ok) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/12345.txt",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(200, static_cast<int>(transport.responseInfo().status()));
  EXPECT_EQ("12345", transport.responseBody());
}

TEST_F(http_HttpFileHandler, GET_fail_access) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/fail-access",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(403, static_cast<int>(transport.responseInfo().status()));
}

TEST_F(http_HttpFileHandler, GET_fail_perm) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/fail-perm",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(403, static_cast<int>(transport.responseInfo().status()));
}

TEST_F(http_HttpFileHandler, HEAD_simple) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "HEAD", "/12345.txt",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(200, static_cast<int>(transport.responseInfo().status()));
  EXPECT_EQ("", transport.responseBody());

  EXPECT_EQ("5", transport.responseInfo().getHeader("Content-Length"));
  EXPECT_EQ(0, transport.responseBody().size());
}

TEST_F(http_HttpFileHandler, GET_range_full) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/12345.txt",
      {{"Host", "test"},
       {"Range", "bytes=0-4"}}, "");

  EXPECT_EQ(HttpStatus::PartialContent, transport.responseInfo().status());
  EXPECT_EQ("bytes 0-4/5", transport.responseInfo().getHeader("Content-Range"));
  EXPECT_EQ("12345", transport.responseBody());
}

TEST_F(http_HttpFileHandler, GET_range_first_n) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/12345.txt",
      {{"Host", "test"},
       {"Range", "bytes=0-1"}}, "");

  EXPECT_EQ(HttpStatus::PartialContent, transport.responseInfo().status());
  EXPECT_EQ("bytes 0-1/5", transport.responseInfo().getHeader("Content-Range"));
  EXPECT_EQ("12", transport.responseBody());
}

TEST_F(http_HttpFileHandler, GET_range_last_n) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/12345.txt",
      {{"Host", "test"},
       {"Range", "bytes=-2"}}, "");

  EXPECT_EQ(HttpStatus::PartialContent, transport.responseInfo().status());
  EXPECT_EQ("bytes 3-4/5", transport.responseInfo().getHeader("Content-Range"));
  EXPECT_EQ("45", transport.responseBody());
}

TEST_F(http_HttpFileHandler, GET_range_many) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/12345.txt",
      {{"Host", "test"},
       {"Range", "bytes=0-0,3-3"}}, "");

  EXPECT_EQ(HttpStatus::PartialContent, transport.responseInfo().status());
  EXPECT_EQ("multipart/byteranges; boundary=HelloBoundaryID", transport.responseInfo().getHeader("Content-Type"));
  EXPECT_EQ("\r\n"
            "--HelloBoundaryID\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Range: bytes 0-0/5\r\n"
            "\r\n"
            "1\r\n"
            "--HelloBoundaryID\r\n"
            "Content-Type: application/octet-stream\r\n"
            "Content-Range: bytes 3-3/5\r\n"
            "\r\n"
            "4\r\n"
            "--HelloBoundaryID--\r\n",
            transport.responseBody());
}

