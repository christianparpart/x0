// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/Http1Connection.h>
#include <xzero/http/BadMessage.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/logging.h>
#include <xzero/HugeBuffer.h>
#include <xzero/Buffer.h>

namespace xzero {
namespace http {
namespace client {

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("http.client.Http1Connection: " msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

Http1Connection::Http1Connection(HttpListener* channel,
                                 TcpEndPoint* endpoint,
                                 Executor* executor)
    : TcpConnection(endpoint, executor),
      channel_(channel),
      onComplete_(),
      writer_(),
      generator_(&writer_),
      parser_(http1::Parser::RESPONSE, this),
      inputBuffer_(),
      inputOffset_(0),
      expectsBody_(true),
      responseComplete_(false),
      keepAliveCount_(0) {
  inputBuffer_.reserve(4096);
}

Http1Connection::~Http1Connection() {
}

void Http1Connection::setListener(HttpListener* channel) {
  channel_ = channel;
}

static void removeConnectionHeaders(HeaderFieldList& headers) { // {{{
  // filter out disruptive connection-level headers
  headers.remove("Connection");
  headers.remove("Content-Length");
  headers.remove("Expect");
  headers.remove("Trailer");
  headers.remove("Transfer-Encoding");
  headers.remove("Upgrade");
} // }}}

void Http1Connection::send(const HttpRequestInfo& requestInfo,
                           CompletionHandler onComplete) {
  HttpRequestInfo req(requestInfo);
  removeConnectionHeaders(req.headers());

  setCompleter(onComplete);
  responseComplete_ = false;
  expectsBody_ = req.method() != HttpMethod::HEAD;
  generator_.generateRequest(req);
  wantWrite();
}

void Http1Connection::send(const HttpRequestInfo& requestInfo,
                           const BufferRef& chunk,
                           CompletionHandler onComplete) {
  HttpRequestInfo req(requestInfo);
  removeConnectionHeaders(req.headers());

  setCompleter(onComplete);
  responseComplete_ = false;
  expectsBody_ = req.method() != HttpMethod::HEAD;
  generator_.generateRequest(req, chunk);
  wantWrite();
}

void Http1Connection::send(const HttpRequestInfo& requestInfo,
                           Buffer&& chunk,
                           CompletionHandler onComplete) {
  HttpRequestInfo req(requestInfo);
  removeConnectionHeaders(req.headers());

  setCompleter(onComplete);
  responseComplete_ = false;
  expectsBody_ = req.method() != HttpMethod::HEAD;
  generator_.generateRequest(req, chunk);
  wantWrite();
}

void Http1Connection::send(const HttpRequestInfo& requestInfo,
                           FileView&& chunk,
                           CompletionHandler onComplete) {
  HttpRequestInfo req(requestInfo);
  removeConnectionHeaders(req.headers());

  setCompleter(onComplete);
  responseComplete_ = false;
  expectsBody_ = req.method() != HttpMethod::HEAD;
  generator_.generateRequest(req, std::move(chunk));
  wantWrite();
}

void Http1Connection::send(const HttpRequestInfo& requestInfo,
                           HugeBuffer&& chunk,
                           CompletionHandler onComplete) {
  HttpRequestInfo req(requestInfo);
  removeConnectionHeaders(req.headers());

  setCompleter(onComplete);
  responseComplete_ = false;
  expectsBody_ = req.method() != HttpMethod::HEAD;
  generator_.generateRequest(req, std::move(chunk));
  wantWrite();
}

void Http1Connection::send(const BufferRef& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  generator_.generateBody(std::move(chunk));
  wantWrite();
}

void Http1Connection::send(Buffer&& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  generator_.generateBody(std::move(chunk));
  wantWrite();
}

void Http1Connection::send(FileView&& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete_);
  generator_.generateBody(std::move(chunk));
  wantWrite();
}

void Http1Connection::send(HugeBuffer&& chunk, CompletionHandler onComplete) {
  if (chunk.isBuffered()) {
    send(chunk.takeBuffer(), onComplete);
  } else {
    send(chunk.takeFileView(), onComplete);
  }
}

void Http1Connection::completed() {
  if (!generator_.isChunked() && generator_.remainingContentLength() > 0)
    throw InvalidState{"Request not fully written but completed() invoked."};

  setCompleter(std::bind(&Http1Connection::onRequestComplete, this,
                         std::placeholders::_1));

  //generator_.generateTrailer(channel_->requestInfo()->trailers());
  wantWrite();
}

void Http1Connection::onRequestComplete(bool success) {
  TRACE("onRequestComplete($0)", success ? "success" : "failed");
  if (success) {
    wantRead();
  }
}

void Http1Connection::onResponseComplete(bool success) {
  // channel_->responseEnd();

  if (!keepAliveCount_) {
    close();
  }
}

void Http1Connection::abort() {
  close();
}

void Http1Connection::onReadable() {
  TRACE("onReadable()");

  size_t n = endpoint()->read(&inputBuffer_);

  if (n == 0) {
    abort();
    return;
  }

  parseFragment();

  if (!responseComplete_) {
    wantRead();
  }
}

void Http1Connection::parseFragment() {
  size_t n = parser_.parseFragment(inputBuffer_.ref(inputOffset_));
  inputOffset_ += n;
}

void Http1Connection::onWriteable() {
  TRACE("onWriteable()");
  const bool complete = writer_.flushTo(endpoint());
  TRACE("onWriteable: $0", complete ? "completed" : "needs-more-to-flush");

  if (!complete) {
    wantWrite();
  } else {
    notifySuccess();
  }
}

void Http1Connection::onInterestFailure(const std::exception& error) {
  //TODO logError("http.client.Http1Connection", error, "Unhandled exception caught in I/O loop");

  notifyFailure(); // notify the callback that we failed doing something wrt. I/O.
  abort();
}

void Http1Connection::setCompleter(CompletionHandler onComplete) {
  if (onComplete && onComplete_)
    throw InvalidState{"There is still another completion hook."};

  onComplete_ = std::move(onComplete);
}

void Http1Connection::invokeCompleter(bool success) {
  if (onComplete_) {
    auto cb = std::move(onComplete_);
    onComplete_ = nullptr;
    cb(success);
  }
}

// {{{ HttpListener overrides
void Http1Connection::onMessageBegin(HttpVersion version,
                                     HttpStatus status,
                                     const BufferRef& text) {
  expectsBody_ = !isContentForbidden(status);
  channel_->onMessageBegin(version, status, text);
}

void Http1Connection::onMessageHeader(const BufferRef& name,
                                      const BufferRef& value) {
  channel_->onMessageHeader(name, value);
}

void Http1Connection::onMessageHeaderEnd() {
  TRACE("onMessageHeaderEnd! expectsBody=$0", expectsBody_);
  channel_->onMessageHeaderEnd();
  if (parser_.isContentExpected() && !expectsBody_) {
    onMessageEnd();
  }
}

void Http1Connection::onMessageContent(const BufferRef& chunk) {
  channel_->onMessageContent(chunk);
}

void Http1Connection::onMessageContent(FileView&& chunk) {
  channel_->onMessageContent(std::move(chunk));
}

void Http1Connection::onMessageEnd() {
  TRACE("onMessageEnd!");
  responseComplete_ = true;
  channel_->onMessageEnd();
}

void Http1Connection::onError(std::error_code ec) {
  TRACE("onError! $0", ec.message());
  channel_->onError(ec);
}
// }}}

} // namespace client
} // namespace http
} // namespace xzero
