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

#define HTTP_DATE_FMT "%a, %d %b %Y %T GMT"

class http_HttpFileHandler : public testing::Test { // {{{
 protected:
  MimeTypes mimetypes_;
  UnixTime mtime_;
  MemoryFileRepository vfs_;
  HttpFileHandler staticFileHandler_;

 public:
  http_HttpFileHandler();
  static std::string generateBoundaryID();
  void staticfileHandler(HttpRequest* request, HttpResponse* response);
  std::shared_ptr<File> getFile(const std::string& path);
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

std::shared_ptr<File> http_HttpFileHandler::getFile(const std::string& path) {
  return vfs_.getFile(path);
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
 * [x] 403, permission failure (EPERM, EACCES)
 * [x] (conditional request) If-None-Match
 * [x] (conditional request) If-Match
 * [x] (conditional request) If-Modified-Since
 * [x] (conditional request) If-Unmodified-Since
 * [x] (conditional request) If-Range
 * [x] (ranged request) full range
 * [x] (ranged request) invalid range (for example "0-4" instead of "range=0-4")
 * [x] (ranged request) first N bytes
 * [x] (ranged request) last N bytes
 * [x] (ranged request) multiple ranges
 * [x] HEAD on basic file
 * [x] HEAD on conditional request
 * [x] HEAD on ranged request
 * [x] non-GET/HEAD (should result in MethodNotAllowed)
 */

TEST_F(http_HttpFileHandler, MethodNotAllowed) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  transport.run(HttpVersion::VERSION_1_1, "POST", path,
      {{"Host", "test"}}, "");
  EXPECT_EQ(HttpStatus::MethodNotAllowed, transport.responseInfo().status());
}

TEST_F(http_HttpFileHandler, GET_FileNotFound) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/notfound.txt",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(HttpStatus::NotFound, transport.responseInfo().status());
}

TEST_F(http_HttpFileHandler, GET_Ok) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/12345.txt",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
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
  EXPECT_EQ(HttpStatus::Forbidden, transport.responseInfo().status());
}

TEST_F(http_HttpFileHandler, GET_fail_perm) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "GET", "/fail-perm",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(HttpStatus::Forbidden, transport.responseInfo().status());
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

TEST_F(http_HttpFileHandler, GET_if_match) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-Match", "__" + file->etag()}}, "");
  EXPECT_EQ(HttpStatus::PreconditionFailed, transport.responseInfo().status());

  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-Match", file->etag()}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
}

TEST_F(http_HttpFileHandler, GET_if_none_match) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-None-Match", file->etag()}}, "");
  EXPECT_EQ(HttpStatus::PreconditionFailed, transport.responseInfo().status());

  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-None-Match", "__" + file->etag()}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
}

TEST_F(http_HttpFileHandler, GET_if_modified_since) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  // test exact-date match
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-Modified-Since", file->lastModified()}}, "");
  EXPECT_EQ(HttpStatus::NotModified, transport.responseInfo().status());

  // test future-date-match
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-Modified-Since", (mtime_ + 1_minutes).toString(HTTP_DATE_FMT)}}, "");
  EXPECT_EQ(HttpStatus::NotModified, transport.responseInfo().status());

  // test past-date match
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-Modified-Since", (mtime_ - 1_minutes).toString(HTTP_DATE_FMT)}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
}

TEST_F(http_HttpFileHandler, GET_if_unmodified_since) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  // test exact-date match
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-Unmodified-Since", file->lastModified()}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
  EXPECT_EQ(5, transport.responseBody().size());

  // test future-date match
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-Unmodified-Since", (mtime_ + 1_minutes).toString(HTTP_DATE_FMT)}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
  EXPECT_EQ(5, transport.responseBody().size());

  // test past-date match
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"If-Unmodified-Since", (mtime_ - 1_minutes).toString(HTTP_DATE_FMT)}}, "");
  EXPECT_EQ(HttpStatus::PreconditionFailed, transport.responseInfo().status());
}

TEST_F(http_HttpFileHandler, GET_if_range_etag) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  // test exact-etag match: receive requested range, as ETag matches
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"Range", "bytes=0-2"},
       {"If-Range", file->etag()}}, "");
  EXPECT_EQ(HttpStatus::PartialContent, transport.responseInfo().status());
  EXPECT_EQ("123", transport.responseBody());

  // test wrong-etag match: receive full message instead
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"Range", "bytes=0-2"},
       {"If-Range", "__" + file->etag()}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
  EXPECT_EQ("12345", transport.responseBody());
}

TEST_F(http_HttpFileHandler, GET_if_range_date) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  // test exact-etag match: receive requested range, as ETag matches
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"Range", "bytes=0-2"},
       {"If-Range", file->lastModified()}}, "");
  EXPECT_EQ(HttpStatus::PartialContent, transport.responseInfo().status());
  EXPECT_EQ("123", transport.responseBody());

  // test wrong-etag match: receive full message instead
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"Range", "bytes=0-2"},
       {"If-Range", (mtime_  - 1_minutes).toString(HTTP_DATE_FMT)}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
  EXPECT_EQ("12345", transport.responseBody());
}

TEST_F(http_HttpFileHandler, GET_range_invalid1) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  // invalid Range header is ignored, we receive full response then.
  transport.run(HttpVersion::VERSION_1_1, "GET", path,
      {{"Host", "test"},
       {"Range", "0-2"}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
  EXPECT_EQ("12345", transport.responseBody());
}

TEST_F(http_HttpFileHandler, HEAD_simple) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "HEAD", "/12345.txt",
      {{"Host", "test"}}, "");

  EXPECT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
  EXPECT_EQ("", transport.responseBody());

  EXPECT_EQ("5", transport.responseInfo().getHeader("Content-Length"));
  EXPECT_EQ(0, transport.responseBody().size());
}

TEST_F(http_HttpFileHandler, HEAD_range_full) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  transport.run(HttpVersion::VERSION_1_1, "HEAD", "/12345.txt",
      {{"Host", "test"},
       {"Range", "bytes=0-4"}}, "");

  EXPECT_EQ(HttpStatus::PartialContent, transport.responseInfo().status());
  EXPECT_EQ("bytes 0-4/5", transport.responseInfo().getHeader("Content-Range"));
  EXPECT_EQ("", transport.responseBody());
}

TEST_F(http_HttpFileHandler, HEAD_if_match) {
  LocalExecutor executor;
  mock::Transport transport(&executor,
      std::bind(&http_HttpFileHandler::staticfileHandler, this,
                std::placeholders::_1, std::placeholders::_2));

  std::string path = "/12345.txt";
  auto file = getFile(path);

  transport.run(HttpVersion::VERSION_1_1, "HEAD", path,
      {{"Host", "test"},
       {"If-Match", "__" + file->etag()}}, "");
  EXPECT_EQ(HttpStatus::PreconditionFailed, transport.responseInfo().status());
  EXPECT_EQ("", transport.responseBody());

  transport.run(HttpVersion::VERSION_1_1, "HEAD", path,
      {{"Host", "test"},
       {"If-Match", file->etag()}}, "");
  EXPECT_EQ(HttpStatus::Ok, transport.responseInfo().status());
  EXPECT_EQ("", transport.responseBody());
}

