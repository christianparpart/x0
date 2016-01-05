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

void Stream::abort() {
  //tTODO ransport_->resetStream(id_);
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

void Stream::sendWindowUpdate(size_t windowSize) {
}

void Stream::appendBody(const BufferRef& data) {
}

void Stream::handleRequest() {
}

void Stream::sendHeaders(const HttpResponseInfo& info) {
}

void Stream::setCompleter(CompletionHandler onComplete) {
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


} // namespace http2
} // namespace http
} // namespace xzero
