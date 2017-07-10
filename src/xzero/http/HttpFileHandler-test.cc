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
#include <xzero/io/LocalFileRepository.h>
#include <xzero/executor/LocalExecutor.h>
#include <xzero/io/FileUtil.h>
#include <xzero/MimeTypes.h>
#include <xzero/Buffer.h>
#include <xzero/testing.h>

using namespace xzero;
using namespace xzero::http;

static std::string generateBoundaryID() {
  return "HelloBoundaryID";
}

void staticfileHandler(HttpRequest* request, HttpResponse* response) {
  MimeTypes mimetypes;
  LocalFileRepository repo(mimetypes, "/", true, true, true);
  HttpFileHandler staticfile(&generateBoundaryID);

  auto docroot = FileUtil::realpath(".");
  auto file = repo.getFile(request->path(), docroot);

  HttpStatus status = staticfile.handle(request, response, file);
  if (!isError(status))
    return;

  response->setStatus(HttpStatus::NotFound);
  response->completed();
}

TEST(http_HttpFileHandler, GET_FileNotFound) {
  LocalExecutor executor;
  mock::Transport transport(&executor, &staticfileHandler);
  transport.run(HttpVersion::VERSION_1_1, "GET", "/notfound.txt",
      {{"Host", "test"}}, "");
  ASSERT_EQ(HttpVersion::VERSION_1_1, transport.responseInfo().version());
  ASSERT_EQ(404, static_cast<int>(transport.responseInfo().status()));
}

