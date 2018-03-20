// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/Context.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpStatus.h>
#include <xzero/net/IPAddress.h>
#include <xzero/WallClock.h>
#include <xzero/UnixTime.h>
#include <xzero/logging.h>
#include <stdexcept>

using namespace xzero;
using namespace xzero::http;

namespace x0d {

Context::Context(
    const xzero::flow::Handler* requestHandler,
    xzero::http::HttpRequest* request,
    xzero::http::HttpResponse* response,
    const std::unordered_map<xzero::http::HttpStatus, std::string>* globalErrorPages,
    size_t maxInternalRedirectCount)
    : requestHandler_(requestHandler),
      runner_(),
      createdAt_(now()),
      requests_({request}),
      response_(response),
      documentRoot_(),
      pathInfo_(),
      file_(),
      errorPages_(),
      globalErrorPages_(globalErrorPages),
      maxInternalRedirectCount_(maxInternalRedirectCount) {
}

Context::Context(const Context& v)
    : requestHandler_(v.requestHandler_),
      runner_(),
      createdAt_(v.createdAt_),
      requests_({v.masterRequest()}),
      response_(v.response_),
      documentRoot_(v.documentRoot_),
      pathInfo_(v.pathInfo_),
      file_(v.file_),
      errorPages_(v.errorPages_),
      globalErrorPages_(v.globalErrorPages_),
      maxInternalRedirectCount_(v.maxInternalRedirectCount_) {
}

Context::~Context() {
  // ensure all internal redirect requests are freed
  while (request() != masterRequest()) {
    delete requests_.front(); // TODO: eliminate the need to explicitly *delete*
    requests_.pop_front();
  }

  clearCustomData();
}

void Context::operator()() {
  handleRequest();
}

void Context::handleRequest() {
  runner_ = std::make_unique<flow::Runner>(requestHandler_);
  runner_->setUserData(this);

  if (request()->expect100Continue()) {
    response()->send100Continue([this](bool succeed) {
      request()->consumeContent(std::bind(&flow::Runner::run, runner()));
    });
  } else {
    request()->consumeContent(std::bind(&flow::Runner::run, runner()));
  }
}

xzero::UnixTime Context::now() const noexcept {
  return WallClock::now();
}

xzero::Duration Context::age() const noexcept {
  return now() - createdAt();
}

const IPAddress& Context::remoteIP() const {
  if (requests_.back()->remoteAddress().isSome())
    return requests_.back()->remoteAddress()->ip();

  throw std::logic_error{"Non-IP transport channels not supported"};
}

int Context::remotePort() const {
  if (requests_.back()->remoteAddress().isSome())
    return requests_.back()->remoteAddress()->port();

  throw std::logic_error{"Non-IP transport channels not supported"};
}

const IPAddress& Context::localIP() const {
  if (requests_.back()->localAddress().isSome())
    return requests_.back()->localAddress()->ip();

  throw std::logic_error{"Non-IP transport channels not supported"};
}

int Context::localPort() const {
  if (requests_.back()->localAddress().isSome())
    return requests_.back()->localAddress()->port();

  throw std::logic_error{"Non-IP transport channels not supported"};
}

size_t Context::bytesReceived() const noexcept {
  return requests_.back()->bytesReceived();
}

size_t Context::bytesTransmitted() const noexcept {
  return response_->bytesTransmitted();
}

void Context::setErrorPage(HttpStatus status, const std::string& path) {
  errorPages_[status] = path;
}

bool Context::getErrorPage(HttpStatus status, std::string* uri) const {
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

bool requiresExternalRedirect(const std::string& uri) {
  return uri[0] != '/';
}

bool Context::sendErrorPage(xzero::http::HttpStatus status,
                                 bool* internalRedirect,
                                 xzero::http::HttpStatus overrideStatus) {
  if (internalRedirect) {
    *internalRedirect = false;
  }

  response_->removeAllHeaders();
  response_->removeAllOutputFilters();

  if (!isError(status)) {
    // no client (4xx) nor server (5xx) error; so just generate simple response
    response_->setStatus(status);
    response_->completed();
    return true;
  }

  std::string uri;
  if (getErrorPage(status, &uri)) {
    if (requiresExternalRedirect(uri)) {
      response_->setStatus(HttpStatus::Found);
      response_->setHeader("Location", uri);
      response_->completed();
      return true;
    } else if (internalRedirectCount() < maxInternalRedirectCount_) {
      if (internalRedirect) {
        *internalRedirect = true;
      }
      runner_->rewind();
      response_->setStatus(!overrideStatus ? status : overrideStatus);
      requests_.emplace_front(new HttpRequest(request()->version(),
                                              "GET",
                                              uri,
                                              request()->headers(),
                                              request()->isSecure(),
                                              {}));
      return false;
    } else {
      logError("Too many internal redirects.");
      sendTrivialResponse(HttpStatus::InternalServerError, "Too many internal redirects.");
      return true;
    }
  } else {
    sendTrivialResponse(status);
    return true;
  }
}

void Context::sendTrivialResponse(HttpStatus status, const std::string& reason) {
  if (isContentForbidden(status)) {
    response_->setStatus(status);
    response_->completed();
    return;
  }

  Buffer body(2048);

  Buffer htmlMessage = reason.empty() ? to_string(status) : reason;

  htmlMessage.replaceAll("<", "&lt;");
  htmlMessage.replaceAll(">", "&gt;");
  htmlMessage.replaceAll("&", "&amp;");

  body << "<DOCTYPE html>\n"
          "<html>\n"
          "  <head>\n"
          "    <title> Error. " << htmlMessage << " </title>\n"
          "  </head>\n"
          "  <body>\n"
          "    <h1> Error. " << htmlMessage << " </h1>\n"
          "  </body>\n"
          "</html>\n";

  response_->setStatus(status);
  response_->setHeader("Cache-Control", "must-revalidate,no-cache,no-store");
  response_->setHeader("Content-Type", "text/html");
  response_->setContentLength(body.size());
  response_->write(std::move(body),
                   std::bind(&HttpResponse::completed, response_));
}

} // namespace x0d
