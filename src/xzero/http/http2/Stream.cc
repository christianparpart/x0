// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
               Stream* parentStream,
               bool exclusiveDependency,
               unsigned weight,
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
      parentStream_(parentStream),
      prevSiblingStream_(nullptr),
      nextSiblingStream_(nullptr),
      firstDependantStream_(nullptr),
      weight_(weight),
      body_(),
      onComplete_() {
  if (parentStream_) {
    if (exclusiveDependency) {
      // reparent all dependent streams from parentStream
      Stream* dependent = parentStream_->firstDependantStream_;
      while (dependent != nullptr) {
        dependent->parentStream_ = this;
        dependent = dependent->nextSiblingStream_;
      }
      parentStream_->firstDependantStream_ = this;
    } else {
      // move this stream into the dependent-stream list of parentStream
      nextSiblingStream_ = parentStream_->firstDependantStream_;
      parentStream_->firstDependantStream_->prevSiblingStream_ = this;
      parentStream_->firstDependantStream_ = this;
    }
  }
}

bool Stream::isAncestor(const Stream* other) const noexcept {
  while (other != nullptr) {
    if (other == this)
      return true;

    other = other->parentStream_;
  }

  return false;
}

bool Stream::isDescendant(const Stream* other) const noexcept {
  return other->isAncestor(this);
}

void Stream::reparent(Stream* newParent, bool exclusive) {
  // RFC 7540, 5.3.3

  if (newParent == this)
    return;

  // if the newParent's is a descendant of this, reparent newParent to our
  // current parent first.
  for (Stream* p = newParent; p != nullptr; p = p->parentStream_) {
    if (p->parentStream_ == this) {
      newParent->parentStream_ = parentStream_;
      break;
    }
  }

  parentStream_ = newParent;

  if (exclusive) {
    if (parentStream_) {
      // reparent dependants to grand-parent
      Stream* dependent = parentStream_->firstDependantStream_;
      parentStream_->firstDependantStream_ = this;
      while (dependent != nullptr) {
        dependent->parentStream_ = this;
        dependent = dependent->nextSiblingStream_;
      }
    } else {
      // reparent to root
      Stream* root = parentStream_;
      while (root->parentStream_ != nullptr)
        root = root->parentStream_;
    }
  } else {
    if (parentStream_) {
    }
  }
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

void Stream::invokeCompleter(bool success) {
  if (onComplete_) {
    TRACE("invoking completion callback");
    auto callback = std::move(onComplete_);
    onComplete_ = nullptr;
    callback(success);
  }
}

void Stream::sendHeaders(const HttpResponseInfo& info) {
  //const bool last = false;
  //TODO connection()->sendHeaders(id_, info.headers());
}

void Stream::closeInput() {
  inputClosed_ = true;
}

void Stream::closeOutput() {
  outputClosed_ = true;
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

template <>
std::string StringUtil::toString(http::http2::Stream* value) {
  char buf[64];
  int n = snprintf(buf, sizeof(buf), "@%p", value);
  return std::string(buf, 0, n);
}

} // namespace xzero
