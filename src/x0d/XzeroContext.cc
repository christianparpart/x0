// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/XzeroContext.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/net/IPAddress.h>
#include <xzero/WallClock.h>
#include <xzero/UnixTime.h>
#include <xzero/logging.h>

using namespace xzero;
using namespace xzero::http;

namespace x0d {

XzeroContext::XzeroContext(
    std::shared_ptr<xzero::flow::vm::Handler> entrypoint,
    xzero::http::HttpRequest* request,
    xzero::http::HttpResponse* response,
    std::unordered_map<xzero::http::HttpStatus, std::string>* globalErrorPages,
    size_t maxInternalRedirectCount)
    : runner_(entrypoint->createRunner()),
      createdAt_(now()),
      requests_({request}),
      response_(response),
      documentRoot_(),
      pathInfo_(),
      file_(),
      errorPages_(),
      globalErrorPages_(globalErrorPages),
      maxInternalRedirectCount_(maxInternalRedirectCount) {
  runner_->setUserData(this);
  response_->onResponseEnd([this] {
    // explicitely wipe customdata before we're actually deleting the context
    clearCustomData();
    delete this;
  });
}

XzeroContext::~XzeroContext() {
  // ensure all internal redirect requests are freed
  while (request() != masterRequest()) {
    delete requests_.front();
    requests_.pop_front();
  }
}

xzero::UnixTime XzeroContext::now() const {
  return WallClock::now();
}

xzero::Duration XzeroContext::age() const {
  return now() - createdAt();
}

const IPAddress& XzeroContext::remoteIP() const {
  if (requests_.back()->remoteAddress().isSome())
    return requests_.back()->remoteAddress()->ip();

  RAISE(RuntimeError, "Non-IP transport channels not supported");
}

int XzeroContext::remotePort() const {
  if (requests_.back()->remoteAddress().isSome())
    return requests_.back()->remoteAddress()->port();

  RAISE(RuntimeError, "Non-IP transport channels not supported");
}

const IPAddress& XzeroContext::localIP() const {
  if (requests_.back()->localAddress().isSome())
    return requests_.back()->localAddress()->ip();

  RAISE(RuntimeError, "Non-IP transport channels not supported");
}

int XzeroContext::localPort() const {
  if (requests_.back()->localAddress().isSome())
    return requests_.back()->localAddress()->port();

  RAISE(RuntimeError, "Non-IP transport channels not supported");
}

size_t XzeroContext::bytesReceived() const {
  return requests_.back()->bytesReceived();
}

size_t XzeroContext::bytesTransmitted() const {
  return response_->bytesTransmitted();
}

bool XzeroContext::verifyDirectoryDepth() {
  if (request()->directoryDepth() < 0) {
    logError("x0d", "Directory traversal detected: $0", request()->path());
    // TODO: why not throwing BadMessage here (anymore?) ?
    //throw BadMessage(HttpStatus::BadRequest, "Directory traversal detected");
    response()->setStatus(HttpStatus::BadRequest);
    response()->setReason("Directory traversal detected");
    response()->completed();
    return false;
  }
  return true;
}

void XzeroContext::setErrorPage(HttpStatus status, const std::string& path) {
  errorPages_[status] = path;
}

bool XzeroContext::getErrorPage(HttpStatus status, std::string* uri) const {
  auto i = errorPages_.find(status);
  if (i != errorPages_.end()) {
    *uri = i->second;
    return true;
  }

  i = globalErrorPages_->find(status);
  if (i != globalErrorPages_->end()) {
    *uri = i->second;
    return true;
  }

  return false;
}

void XzeroContext::internalRedirect(const std::string& uri) {
  requests_.emplace_front(new HttpRequest(
        "GET",
        uri,
        request()->version(),
        request()->isSecure(),
        request()->headers(),
        Buffer()));
  runner_->rewind();
}

void XzeroContext::sendErrorPage(xzero::http::HttpStatus status, bool* rewind) {
  *rewind = false;

  if (status == HttpStatus::NoResponse) {
    response_->completed(); // TODO: response_->abort();
    return;
  }

  if (!isError(status)) {
    // no client (4xx) nor server (5xx) error; so just generate simple response
    response_->setStatus(status);
    response_->completed();
    return;
  }

  std::string uri;
  if (getErrorPage(status, &uri)) {
    if (internalRedirectCount() < maxInternalRedirectCount_) {
      internalRedirect(uri);
      *rewind = true;
      return;
    } else {
      // send plain 500 "too many internal redirects" instead
      response_->setStatus(HttpStatus::InternalServerError);
      response_->setReason("Too many internal redirects");
      logError("x0d", "Too many internal redirects.");
    }
  }

  // send plain 500 "too many internal redirects" instead
  response_->setStatus(HttpStatus::InternalServerError);

  *rewind = false;
  response_->setStatus(status);

  if (!isContentForbidden(status)) {
  }

  response_->completed();
}

} // namespace x0d
