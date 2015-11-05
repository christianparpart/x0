// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpBufferedInput.h>
#include <xzero/http/HttpInputListener.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

namespace xzero {
namespace http {

// TODO support buffering input into a temp file (via O_TMPFILE if available)

#ifndef NDEBUG
#define TRACE(msg...) logTrace("http.HttpBufferedInput", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

HttpBufferedInput::HttpBufferedInput()
    : HttpInput(),
      content_(),
      offset_(0) {
  TRACE("$0 ctor", this);
}

HttpBufferedInput::~HttpBufferedInput() {
  TRACE("$0 dtor", this);
}

void HttpBufferedInput::recycle() {
  TRACE("$0 recycle", this);
  content_.clear();
  offset_ = 0;
}

int HttpBufferedInput::read(Buffer* result) {
  const size_t len = content_.size() - offset_;
  result->push_back(content_.ref(offset_));
  TRACE("$0 read: $1 bytes", this, len);

  content_.clear();
  offset_ = 0;

  return len;
}

size_t HttpBufferedInput::readLine(Buffer* result) {
  const size_t len = content_.size() - offset_;
  TRACE("$0 readLine: $1 bytes", this, len);

  const size_t n = content_.find('\n', offset_);
  if (n == Buffer::npos) {
    result->push_back(content_);
    content_.clear();
    offset_ = 0;
    return len;
  }

  result->push_back(content_.ref(offset_, n - offset_));
  offset_ = n + 1;

  if (offset_ == content_.size()) {
    content_.clear();
    offset_ = 0;
  }

  return 0;
}

bool HttpBufferedInput::empty() const noexcept {
  return offset_ == content_.size();
}

void HttpBufferedInput::onContent(const BufferRef& chunk) {
  TRACE("$0 onContent: $1 bytes", this, chunk.size());
  content_ += chunk;

  if (listener())
    listener()->onContentAvailable();
}

} // namespace http

template<>
std::string StringUtil::toString(http::HttpBufferedInput* value) {
  return StringUtil::format("HttpBufferedInput[$0]", (void*)value);
}

} // namespace xzero
