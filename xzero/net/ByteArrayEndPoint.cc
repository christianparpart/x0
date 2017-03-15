// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/net/ByteArrayEndPoint.h>
#include <xzero/net/Connection.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>
#include <system_error>
#include <stdio.h>
#include <unistd.h>

namespace xzero {

#ifndef NDEBUG
#define TRACE(msg...) logTrace("net.ByteArrayEndPoint", msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

ByteArrayEndPoint::ByteArrayEndPoint()
    : input_(),
      readPos_(0),
      output_(),
      closed_(false) {
}

ByteArrayEndPoint::~ByteArrayEndPoint() {
}

void ByteArrayEndPoint::setInput(Buffer&& input) {
  readPos_ = 0;
  input_ = std::move(input);
}

void ByteArrayEndPoint::setInput(const char* input) {
  readPos_ = 0;
  input_.clear();
  input_.push_back(input);
}

const Buffer& ByteArrayEndPoint::input() const {
  return input_;
}

const Buffer& ByteArrayEndPoint::output() const {
  return output_;
}

void ByteArrayEndPoint::close() {
  TRACE("close()");
  // FIXME maybe we need closedInput | closedOutput distinction
  closed_ = true;
}

bool ByteArrayEndPoint::isOpen() const {
  return !closed_;
}

std::string ByteArrayEndPoint::toString() const {
  char buf[256];
  snprintf(buf, sizeof(buf),
           "ByteArrayEndPoint@%p(readPos:%zu, writePos:%zu, closed:%s)",
           this, readPos_, output_.size(),
           closed_ ? "true" : "false");
  return buf;
}

size_t ByteArrayEndPoint::fill(Buffer* sink, size_t count) {
  if (closed_) {
    return 0;
  }

  const size_t bytesAvailable = input_.size() - readPos_;
  const size_t actualBytes = std::min(bytesAvailable, count);

  sink->push_back(input_.ref(readPos_, actualBytes));
  readPos_ += actualBytes;
  TRACE("$0 fill: $1 bytes", this, actualBytes);
  return actualBytes;
}

size_t ByteArrayEndPoint::flush(const BufferRef& source) {
  TRACE("$0 flush: $1 bytes", this, source.size());

  if (closed_) {
    return 0;
  }

  size_t n = output_.size();
  output_.push_back(source);
  return output_.size() - n;
}

size_t ByteArrayEndPoint::flush(int fd, off_t offset, size_t size) {
  output_.reserve(output_.size() + size);

  ssize_t n = pread(fd, output_.end(), size, offset);

  if (n < 0)
    throw std::system_error(errno, std::system_category());

  output_.resize(output_.size() + n);
  return n;
}

void ByteArrayEndPoint::wantFill() {
  if (connection()) {
    TRACE("$0 wantFill.", this);
    ref();
    connection()->executor()->execute([this] {
      TRACE("$0 wantFill: fillable.", this);
      try {
        connection()->onFillable();
      } catch (std::exception& e) {
        connection()->onInterestFailure(e);
      }
      unref();
    });
  }
}

void ByteArrayEndPoint::wantFlush() {
  if (connection()) {
    TRACE("$0 wantFlush.", this);
    ref();
    connection()->executor()->execute([this] {
      TRACE("$0 wantFlush: flushable.", this);
      try {
        connection()->onFlushable();
      } catch (std::exception& e) {
        connection()->onInterestFailure(e);
      }
      unref();
    });
  }
}

Duration ByteArrayEndPoint::readTimeout() {
  return Duration::Zero; // TODO
}

Duration ByteArrayEndPoint::writeTimeout() {
  return Duration::Zero; // TODO
}

void ByteArrayEndPoint::setReadTimeout(Duration timeout) {
  // TODO
}

void ByteArrayEndPoint::setWriteTimeout(Duration timeout) {
  // TODO
}

bool ByteArrayEndPoint::isBlocking() const {
  return false;
}

void ByteArrayEndPoint::setBlocking(bool enable) {
  throw std::system_error(ENOTSUP, std::system_category());
}

bool ByteArrayEndPoint::isCorking() const {
  return false;
}

void ByteArrayEndPoint::setCorking(bool /*enable*/) {
}

bool ByteArrayEndPoint::isTcpNoDelay() const {
  return false;
}

void ByteArrayEndPoint::setTcpNoDelay(bool /*enable*/) {
}

template<>
std::string StringUtil::toString(ByteArrayEndPoint* value) {
  return StringUtil::format("ByteArrayEndPoint[$0]", (void*)value);
}

} // namespace xzero
