// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpChannel.h>
#include <xzero/RuntimeError.h>
#include <xzero/io/FileView.h>
#include <xzero/io/Filter.h>
#include <xzero/sysconfig.h>
#include <cstring>
#include <system_error>
#include <stdexcept>
#include <vector>
#include <string>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

namespace xzero {
namespace http {

HttpResponse::HttpResponse(HttpChannel* channel)
    : channel_(channel),
      info_(),
      committed_(false),
      bytesTransmitted_(0),
      actualContentLength_(0) {
  //.
}

Executor* HttpResponse::executor() const noexcept {
  return channel_->executor();
}

void HttpResponse::recycle() {
  committed_ = false;
  info_.reset();
  bytesTransmitted_ = 0;
  actualContentLength_ = 0;
}

void HttpResponse::requireMutableInfo() {
  if (isCommitted())
    RAISE(IllegalStateError, "Response already committed.");

  requireNotSendingAlready();
}

void HttpResponse::requireNotSendingAlready() {
  switch (channel_->state()) {
    case HttpChannelState::READING:
    case HttpChannelState::HANDLING:
      break;
    case HttpChannelState::SENDING:
    default:
      // "Attempt to modify response while in wrong channel state."
      RAISE(IllegalStateError, "Require not sending already.");
  }
}

void HttpResponse::setCommitted(bool value) {
  committed_ = value;
}

HttpVersion HttpResponse::version() const noexcept {
  return info_.version();
}

void HttpResponse::setVersion(HttpVersion version) {
  requireMutableInfo();
  info_.setVersion(version);
}

void HttpResponse::setStatus(HttpStatus status) {
  requireMutableInfo();
  info_.setStatus(status);
}

void HttpResponse::setReason(const std::string& val) {
  requireMutableInfo();
  info_.setReason(val);
}

void HttpResponse::setContentLength(size_t size) {
  requireMutableInfo();
  info_.setContentLength(size);
}

void HttpResponse::resetContentLength() {
  requireMutableInfo();
  info_.resetContentLength();
}

static const std::vector<std::string> connectionHeaderFields = {
  "Connection",
  "Content-Length",
  "Close",
  "Keep-Alive",
  "TE",
  "Trailer",
  "Transfer-Encoding",
  "Upgrade",
};

inline void requireValidHeader(const std::string& name) {
  for (const auto& test: connectionHeaderFields)
    if (iequals(name, test)) {
      RAISE(InvalidArgumentError);
    }
}

void HttpResponse::addHeader(const std::string& name,
                             const std::string& value) {
  requireMutableInfo();
  requireValidHeader(name);

  headers().push_back(name, value);
}

void HttpResponse::appendHeader(const std::string& name,
                                const std::string& value,
                                const std::string& delim) {
  requireMutableInfo();
  requireValidHeader(name);

  headers().append(name, value, delim);
}

void HttpResponse::prependHeader(const std::string& name,
                                 const std::string& value,
                                 const std::string& delim) {
  requireMutableInfo();
  requireValidHeader(name);

  headers().prepend(name, value, delim);
}

void HttpResponse::setHeader(const std::string& name,
                             const std::string& value) {
  requireMutableInfo();
  requireValidHeader(name);

  headers().overwrite(name, value);
}

void HttpResponse::removeHeader(const std::string& name) {
  requireMutableInfo();

  headers().remove(name);
}

void HttpResponse::removeAllHeaders() {
  requireMutableInfo();

  headers().reset();
}

const std::string& HttpResponse::getHeader(const std::string& name) const {
  return headers().get(name);
}

bool HttpResponse::hasHeader(const std::string& name) const {
  return headers().contains(name);
}

void HttpResponse::send100Continue(CompletionHandler onComplete) {
  channel_->send100Continue(onComplete);
}

void HttpResponse::sendError(HttpStatus code, const std::string& message) {
  if (!isError(code))
    RAISE(InvalidArgumentError);

  // TODO: customizability of error pages - XzeroContext::sendErrorPage

  setStatus(code);
  setReason(message);
  completed();
}

// {{{ trailers
void HttpResponse::registerTrailer(const std::string& name) {
  requireMutableInfo();
  requireValidHeader(name);

  if (info_.trailers().contains(name))
    // "Trailer already registered."
    RAISE(InvalidArgumentError);

  info_.trailers().push_back(name, "");
}

void HttpResponse::appendTrailer(const std::string& name,
                                 const std::string& value,
                                 const std::string& delim) {
  requireNotSendingAlready();
  requireValidHeader(name);

  if (!info_.trailers().contains(name))
    RAISE(IllegalStateError, "Trailer not registered yet.");

  info_.trailers().append(name, value, delim);
}

void HttpResponse::setTrailer(const std::string& name, const std::string& value) {
  requireNotSendingAlready();
  requireValidHeader(name);

  if (!info_.trailers().contains(name))
    RAISE(IllegalStateError, "Trailer not registered yet.");

  info_.trailers().overwrite(name, value);
}
// }}}

void HttpResponse::onPostProcess(std::function<void()> callback) {
  channel_->onPostProcess(callback);
}

void HttpResponse::onResponseEnd(std::function<void()> callback) {
  channel_->onResponseEnd(callback);
}

void HttpResponse::completed() {
  channel_->completed();
}

// {{{ HTTP response content builders
void HttpResponse::addOutputFilter(std::shared_ptr<Filter> filter) {
  channel_->addOutputFilter(filter);
}

void HttpResponse::removeAllOutputFilters() {
  channel_->removeAllOutputFilters();
}

void HttpResponse::write(const char* cstr, CompletionHandler&& completed) {
  const size_t slen = strlen(cstr);
  write(BufferRef(cstr, slen), std::move(completed));
}

void HttpResponse::write(const std::string& str, CompletionHandler&& completed) {
  write(Buffer(str), std::move(completed));
}

void HttpResponse::write(Buffer&& data, CompletionHandler&& completed) {
  actualContentLength_ += data.size();
  channel_->send(std::move(data), std::move(completed));
}

void HttpResponse::write(const BufferRef& data, CompletionHandler&& completed) {
  actualContentLength_ += data.size();
  channel_->send(data, std::move(completed));
}

void HttpResponse::write(FileView&& input, CompletionHandler&& completed) {
  actualContentLength_ += input.size();
  channel_->send(std::move(input), std::move(completed));
}
// }}}

}  // namespace http
}  // namespace xzero
