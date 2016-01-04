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

Stream::Stream(Connection* connection, StreamID id,
               const HttpHandler& handler),
    : connection_(connection),
      channel_(new HttpChannel(this)),
      id_(id),
      state_(StreamState::Idle),
      weight_(16),
      //StreamTreeNode* node_;
      handler_(handler),
      body_(16384),
      onComplete_() {
}

void Stream::abort() {
  transport_->resetStream(id_);
}

void Stream::completed() {
  // ensure last DATA packet has with END_STREAM flag set
  transport_->completed(id_);
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
