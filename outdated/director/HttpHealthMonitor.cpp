// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "HttpHealthMonitor.h"
#include "Director.h"
#include "Backend.h"
#include <cassert>
#include <cstdarg>

using namespace base;
using namespace xzero;

#if 0  // !defined(NDEBUG)
#define TRACE(n, msg...) (this->log(LogMessage(Severity::trace##n, msg)))
#else
#define TRACE(n, msg...) \
  do {                   \
  } while (0)
#endif

HttpHealthMonitor::HttpHealthMonitor(HttpWorker& worker)
    : HealthMonitor(worker),
      socket_(worker_.loop()),
      request_(),
      writeOffset_(0),
      response_() {}

HttpHealthMonitor::~HttpHealthMonitor() {}

void HttpHealthMonitor::reset() {
  HealthMonitor::reset();

  socket_.close();

  writeOffset_ = 0;
  response_.clear();
}

/**
 * Sets the raw HTTP request, used to perform the health check.
 */
void HttpHealthMonitor::setRequest(const char* fmt, ...) {
  va_list va;
  size_t blen = std::min(request_.capacity(), static_cast<size_t>(1023));

  do {
    request_.reserve(blen + 1);
    va_start(va, fmt);
    blen = vsnprintf(const_cast<char*>(request_.data()), request_.capacity(),
                     fmt, va);
    va_end(va);
  } while (blen >= request_.capacity());

  request_.resize(blen);
}

/**
 * Callback, timely invoked when a health check is to be started.
 */
void HttpHealthMonitor::onCheckStart() {
  TRACE(1, "onCheckStart()");

  socket_.open(backend_->socketSpec(), O_NONBLOCK | O_CLOEXEC);

  if (!socket_.isOpen()) {
    // log(LogMessage(Severity::error, "Could not open socket. %s",
    // strerror(errno)));
    logFailure();
  } else if (socket_.state() == Socket::Connecting) {
    TRACE(1, "connecting asynchronously.");
    socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(
        this, backend_->manager()->connectTimeout());
    socket_.setReadyCallback<HttpHealthMonitor,
                             &HttpHealthMonitor::onConnectDone>(this);
    socket_.setMode(Socket::ReadWrite);
  } else {
    socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(
        this, backend_->manager()->writeTimeout());
    socket_.setReadyCallback<HttpHealthMonitor, &HttpHealthMonitor::io>(this);
    socket_.setMode(Socket::ReadWrite);
    TRACE(1, "connected.");
  }
}

/**
 * Callback, invoked on completed asynchronous connect-operation.
 */
void HttpHealthMonitor::onConnectDone(Socket*, int revents) {
  TRACE(1, "onConnectDone($0)", revents);

  if (socket_.state() == Socket::Operational) {
    TRACE(1, "connected");
    socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(
        this, backend_->manager()->writeTimeout());
    socket_.setReadyCallback<HttpHealthMonitor, &HttpHealthMonitor::io>(this);
    socket_.setMode(Socket::ReadWrite);
  } else {
    // log(LogMessage(Severity::error, "Connecting to backend failed. %s",
    // strerror(errno)));
    logFailure();
  }
}

/**
 * Callback, invoked on I/O readiness of origin server connection.
 */
void HttpHealthMonitor::io(Socket*, int revents) {
  TRACE(1, "io($0)", revents);

  if (revents & ev::WRITE) {
    writeSome();
  }

  if (revents & ev::READ) {
    readSome();
  }
}

/**
 * Writes the request chunk to the origin server.
 */
void HttpHealthMonitor::writeSome() {
  TRACE(1, "writeSome()");

  size_t chunkSize = request_.size() - writeOffset_;
  ssize_t writeCount = socket_.write(request_.data() + writeOffset_, chunkSize);

  if (writeCount < 0) {
    // log(LogMessage(Severity::error, "Writing to backend failed. %s",
    // strerror(errno)));
    logFailure();
  } else {
    writeOffset_ += writeCount;

    if (writeOffset_ == request_.size()) {
      socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(
          this, backend_->manager()->readTimeout());
      socket_.setMode(Socket::Read);
    }
  }
}

/**
 * Reads and processes a response chunk from origin server.
 */
void HttpHealthMonitor::readSome() {
  TRACE(1, "readSome()");

  size_t lower_bound = response_.size();
  if (lower_bound == response_.capacity())
    response_.setCapacity(lower_bound + 4096);

  ssize_t rv = socket_.read(response_);

  if (rv > 0) {
    TRACE(1, "readSome: read $0 bytes", rv);
    size_t np = parseFragment(response_.ref(lower_bound, rv));

    (void) np;
    TRACE(1, "readSome(): processed $0 of $1 bytes ($2)",
          np, rv, HttpMessageParser::state());

    if (HttpMessageParser::state() == HttpMessageParser::PROTOCOL_ERROR) {
      TRACE(1, "protcol error");
      logFailure();
    } else if (processingDone_) {
      TRACE(1, "processing done");
      logSuccess();
    } else {
      TRACE(1, "resume with io:$0, state:$1", socket_.mode(), state());
      socket_.setTimeout<HttpHealthMonitor, &HttpHealthMonitor::onTimeout>(
          this, backend_->manager()->readTimeout());
      socket_.setMode(Socket::Read);
    }
  } else if (rv == 0) {
    if (isContentExpected()) {
      onMessageEnd();
    } else {
      TRACE(1, "remote endpoint closed connection.");
      logFailure();
    }
  } else {
    switch (errno) {
      case EAGAIN:
      case EINTR:
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
      case EWOULDBLOCK:
#endif
        break;
      default:
        TRACE(1, "error reading health-check response from backend. $0",
              strerror(errno));
        logFailure();
        return;
    }
  }
}

/**
 * Origin server timed out in read or write operation.
 */
void HttpHealthMonitor::onTimeout(Socket* s) {
  TRACE(1, "onTimeout()");
  // log(LogMessage(Severity::error, "Backend timed out."));
  logFailure();
}
