// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "webdav.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/io/OutputStream.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/File.h>
#include <xzero/logging.h>

/*
 * module webdav
 * =============
 *
 * RFC: 4918
 */

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

namespace x0d {

WebdavModule::WebdavModule(x0d::XzeroDaemon* d)
    : XzeroModule(d, "webdav") {

  mainHandler("webdav", &WebdavModule::webdav)
      .param<int>("access", 0600);
}

bool WebdavModule::webdav(XzeroContext* cx, Params& args) {
  switch (cx->request()->method()) {
    case HttpMethod::PROPFIND:    // 9.1
      return todo(cx);
    case HttpMethod::PROPPATCH:   // 9.3
      return todo(cx);
    case HttpMethod::MKCOL:       // 9.3
      return webdav_mkcol(cx);
    case HttpMethod::GET:         // 9.4
      return webdav_get(cx);
    case HttpMethod::POST:        // 9.5
      return todo(cx);
    case HttpMethod::DELETE:      // 9.6
      return todo(cx);
    case HttpMethod::PUT:         // 9.7
      return webdav_put(cx, args);
    case HttpMethod::COPY:        // 9.8
      return todo(cx);
    case HttpMethod::MOVE:        // 9.9
      return todo(cx);
    case HttpMethod::LOCK:        // 9.10
      return todo(cx);
    case HttpMethod::UNLOCK:      // 9.11
      return todo(cx);
    default:
      return false;
  }
}

bool WebdavModule::webdav_mkcol(XzeroContext* cx) {
  if (!cx->file())
    return false;

  if (!cx->verifyDirectoryDepth())
    return true;

  if (cx->file()->isDirectory()) {
    cx->response()->setStatus(HttpStatus::Ok);
    cx->response()->completed();
    return true;
  }

  logDebug("webdav", "Creating directory: $0", cx->file()->path());
  try {
    FileUtil::mkdir_p(cx->file()->path());
    cx->response()->setStatus(HttpStatus::Created);
  } catch (const std::exception& e) {
    logError("webdav", "Failed creating file $0. $1",
             cx->file()->path(),
             e.what());
    cx->response()->setStatus(HttpStatus::NoContent);
  }
  cx->response()->completed();

  return true;
}

bool WebdavModule::webdav_get(XzeroContext* cx) {
  if (!cx->verifyDirectoryDepth())
    return true;

  HttpStatus status = daemon().fileHandler().handle(cx->request(),
                                                    cx->response(),
                                                    cx->file());
  if (!isError(status)) {
    return true;
  } else {
    bool internalRedirect = false;
    cx->sendErrorPage(status, &internalRedirect);
    return internalRedirect == false;
  }
}

bool WebdavModule::webdav_put(XzeroContext* cx, Params& args) {
  // TODO: pre-allocate full storage in advance
  // TODO: attempt native file rename/move into target location if possible

  if (!cx->file())
    return false;

  // 9.7.2. PUT for Collections
  if (cx->file()->isDirectory()) {
    cx->response()->setStatus(HttpStatus::MethodNotAllowed);
    cx->response()->completed();
    return true;
  }

  if (!cx->verifyDirectoryDepth())
    return true;

  BufferRef content = cx->request()->getContentBuffer();

  logDebug("webdav", "put filename: $0", cx->file()->path());

  //bool didNotExistBefore = !cx->file()->exists();
  int mode = args.getInt(1);
  File::OpenFlags flags = File::Write | File::Create | File::Truncate;
  std::unique_ptr<OutputStream> output = cx->file()->createOutputChannel(flags, mode);

  // if (!output->tryAllocate(content.size())) {
  //   if (didNotExistBefore)
  //     FileUtil::rm(cx->file()->path());
  //   cx->response()->setStatus(HttpStatus::InsufficientStorage);
  //   cx->response()->completed();
  // }

  output->write(content.data(), content.size());

  cx->response()->setStatus(HttpStatus::Created);
  cx->response()->completed();

  return true;
}

bool WebdavModule::todo(XzeroContext* cx) {
  cx->response()->setStatus(HttpStatus::NotImplemented);
  cx->response()->completed();
  return true;
}

} // namespace x0d
