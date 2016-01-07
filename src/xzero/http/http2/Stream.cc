// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2016 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/Stream.h>
#include <xzero/http/http2/Connection.h>
#include <xzero/http/HttpChannel.h>
#include <xzero/io/DataChain.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace http2 {

#define TRACE(msg...) logTrace("http.http2.Stream", msg)

Stream::Stream(StreamID id,
               Connection* connection,
               Executor* executor,
               const HttpHandler& handler,
               size_t maxRequestUriLength,
               size_t maxRequestBodyLength,
               HttpDateGenerator* dateGenerator,
               HttpOutputCompressor* outputCompressor)
    : connection_(connection),
      channel_(new HttpChannel(this,
                               executor,
                               handler,
                               maxRequestUriLength,
                               maxRequestBodyLength,
                               dateGenerator,
                               outputCompressor)),
      id_(id),
      state_(StreamState::Idle),
      weight_(16),
      //StreamTreeNode* node_;
      body_(),
      onComplete_() {
}

void Stream::sendWindowUpdate(size_t windowSize) {
}

void Stream::appendBody(const BufferRef& data) {
  channel_->onMessageContent(data);
}

void Stream::handleRequest() {
}

void Stream::setCompleter(CompletionHandler cb) {
  if (cb && onComplete_)
    RAISE(IllegalStateError, "There is still another completion hook.");

  onComplete_ = std::move(cb);
}

void Stream::sendHeaders(const HttpResponseInfo& info) {
  //const bool last = false;
  //TODO connection()->sendHeaders(id_, info.headers());
}

void Stream::close() {
  // XXX should be already in StreamState::HalfClosedRemote
  state_ = StreamState::Closed;
  switch (state_) {
    case StreamState::Idle:
    case StreamState::Open:
    case StreamState::ReservedRemote:
    case StreamState::ReservedLocal:
    case StreamState::HalfClosedLocal:
      // TODO: what's the state *valid* change?
      break;
    case StreamState::HalfClosedRemote:
      state_ = StreamState::Closed;
      break;
    case StreamState::Closed:
      break;
  }
}

void Stream::abort() {
  //TODO transport_->resetStream(id_);
}

void Stream::completed() {
  // ensure last DATA packet has with END_STREAM flag set
  //TODO transport_->completed(id_);
}

void Stream::send(HttpResponseInfo& responseInfo, Buffer&& chunk,
          CompletionHandler onComplete) {
  setCompleter(onComplete);
  sendHeaders(responseInfo);
  body_.write(std::move(chunk));
}

void Stream::send(HttpResponseInfo& responseInfo, const BufferRef& chunk,
          CompletionHandler onComplete) {
  setCompleter(onComplete);
  sendHeaders(responseInfo);
  body_.write(chunk);
}

void Stream::send(HttpResponseInfo& responseInfo, FileView&& chunk,
          CompletionHandler onComplete) {
  setCompleter(onComplete);
  body_.write(std::move(chunk));
}

void Stream::send(Buffer&& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  body_.write(std::move(chunk));
}

void Stream::send(const BufferRef& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  body_.write(chunk);
}

void Stream::send(FileView&& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  body_.write(std::move(chunk));
}

} // namespace http2
} // namespace http
} // namespace xzero
