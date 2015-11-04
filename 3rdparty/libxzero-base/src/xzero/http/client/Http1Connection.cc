// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/client/Http1Connection.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/net/EndPoint.h>
#include <xzero/logging.h>
#include <xzero/Buffer.h>

namespace xzero {
namespace http {
namespace client {

Http1Connection::Http1Connection(HttpListener* channel,
                                 EndPoint* endpoint,
                                 Executor* executor)
    : Connection(endpoint, executor),
      channel_(channel),
      onComplete_(),
      writer_(),
      generator_(&writer_),
      parser_(http1::Parser::RESPONSE, channel_),
      inputBuffer_(),
      inputOffset_(0),
      keepAliveCount_(0) {
}

Http1Connection::~Http1Connection() {
}

void Http1Connection::send(HttpRequestInfo&& requestInfo,
                           CompletionHandler onComplete) {
  setCompleter(onComplete);
  generator_.generateRequest(requestInfo);
  wantFlush();
}

void Http1Connection::send(HttpRequestInfo&& requestInfo,
                           const BufferRef& chunk,
                           CompletionHandler onComplete) {
  setCompleter(onComplete);
  generator_.generateRequest(requestInfo, chunk);
  wantFlush();
}

void Http1Connection::send(HttpRequestInfo&& requestInfo,
                           Buffer&& chunk,
                           CompletionHandler onComplete) {
  setCompleter(onComplete);
  generator_.generateRequest(requestInfo, chunk);
  wantFlush();
}

void Http1Connection::send(HttpRequestInfo&& requestInfo,
                           FileRef&& chunk,
                           CompletionHandler onComplete) {
  setCompleter(onComplete);
  generator_.generateRequest(requestInfo, std::move(chunk));
  wantFlush();
}

void Http1Connection::send(const BufferRef& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  generator_.generateBody(std::move(chunk));
  wantFlush();
}

void Http1Connection::send(Buffer&& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  generator_.generateBody(std::move(chunk));
  wantFlush();
}

void Http1Connection::send(FileRef&& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete_);
  generator_.generateBody(std::move(chunk));
  wantFlush();
}

void Http1Connection::completed() {
  setCompleter(std::bind(&Http1Connection::onRequestComplete, this,
                         std::placeholders::_1));

  if (!generator_.isChunked() && generator_.pendingContentLength() > 0)
    RAISE(IllegalStateError, "Invalid State. Request not fully written but completed() invoked.");

  //generator_.generateTrailer(channel_->requestInfo()->trailers());
  wantFlush();
}

void Http1Connection::onRequestComplete(bool success) {
  //channel_->responseEnd();

  if (!keepAliveCount_) {
    close();
  }
}

void Http1Connection::abort() {
  endpoint()->close();
}

void Http1Connection::onOpen() {
  Connection::onOpen();
}

void Http1Connection::onClose() {
  Connection::onClose();
}

void Http1Connection::setInputBufferSize(size_t size) {
  inputBuffer_.reserve(size);
}

void Http1Connection::onFillable() {
  if (endpoint()->fill(&inputBuffer_) == 0) {
    abort();
    return;
  }

  parseFragment();
}

void Http1Connection::parseFragment() {
  try {
    size_t n = parser_.parseFragment(inputBuffer_.ref(inputOffset_));
    inputOffset_ += n;
  } catch (const std::exception& e) {
    RAISE(ForeignError);
  }
}

void Http1Connection::onFlushable() {
  const bool complete = writer_.flush(endpoint());

  if (complete) {
    wantFill();
  } else {
    wantFlush();
  }
}

void Http1Connection::onInterestFailure(const std::exception& error) {
  logError("http.client.Http1Connection", error, "Unhandled exception caught in I/O loop");

  notifyFailure(); // notify the callback that we failed doing something wrt. I/O.
  abort();
}

void Http1Connection::setCompleter(CompletionHandler onComplete) {
  if (onComplete && onComplete_)
    RAISE(IllegalStateError, "There is still another completion hook.");

  onComplete_ = std::move(onComplete);
}

void Http1Connection::invokeCompleter(bool success) {
  if (onComplete_) {
    auto cb = std::move(onComplete_);
    onComplete_ = nullptr;
    cb(success);
  }
}

} // namespace client
} // namespace http
} // namespace xzero
