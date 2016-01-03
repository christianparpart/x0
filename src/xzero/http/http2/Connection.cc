// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/http2/Connection.h>
#include <xzero/net/EndPoint.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace http2 {

#define TRACE(msg...) logNotice("http2.Connection", msg)

Connection::Connection(EndPoint* ep,
                       Executor* executor,
                       const HttpHandler& handler,
                       HttpDateGenerator* dateGenerator,
                       HttpOutputCompressor* outputCompressor,
                       size_t maxRequestBodyLength,
                       size_t maxRequestCount)
    : xzero::Connection(ep, executor),
      inputBuffer_(),
      inputOffset_(0),
      parser_(this),
      writer_(),
      generator_(writer_.chain()),
      lowestStreamIdLocal_(0),
      lowestStreamIdRemote_(0),
      maxStreamIdLocal_(0),
      maxStreamIdRemote_(0),
      streams_()
{
  TRACE("ctor");
}

Connection::~Connection() {
  TRACE("dtor");
}

// {{{ net::Connection overrides
void Connection::onOpen() {
  TRACE("onOpen");
  ::xzero::Connection::onOpen();

  // send initial server connection preface
  generator_.generateSettings({}); // leave settings at defaults
  wantFlush();
}

void Connection::onClose() {
  TRACE("onClose");
}

void Connection::setInputBufferSize(size_t size) {
  TRACE("setInputBufferSize");
}

void Connection::onFillable() {
  TRACE("onFillable");

  TRACE("onFillable: calling fill()");
  if (endpoint()->fill(&inputBuffer_) == 0) {
    TRACE("onFillable: fill() returned 0");
    // RAISE("client EOF");
    abort();
    return;
  }

  parseFragment();
}

void Connection::parseFragment() {
  size_t nparsed = parser_.parseFragment(inputBuffer_.ref(inputOffset_));
  inputOffset_ += nparsed;

  if (inputOffset_ == inputBuffer_.size()) {
    inputBuffer_.clear();
    inputOffset_ = 0;
  }

  // TODO: if no interest assigned yet, wantRead then
}

void Connection::onFlushable() {
  TRACE("onFlushable");

  const bool complete = writer_.flush(endpoint());

  if (complete) {
    wantFill();
  } else {
    wantFlush();
  }
}

void Connection::onInterestFailure(const std::exception& error) {
  TRACE("onInterestFailure");
}
// }}}
// {{{ FrameListener overrides
void Connection::onData(StreamID sid, const BufferRef& data, bool last) {
}

void Connection::onRequestBegin(StreamID sid, bool noContent,
                                HttpRequestInfo&& info) {
}

void Connection::onPriority(StreamID sid,
                bool isExclusiveDependency,
                StreamID streamDependency,
                unsigned weight) {
}

void Connection::onPing(const BufferRef& data) {

}

void Connection::onPingAck(const BufferRef& data) {
}

void Connection::onGoAway(StreamID sid, ErrorCode errorCode,
                          const BufferRef& debugData) {
  endpoint()->close();
}

void Connection::onResetStream(StreamID sid, ErrorCode errorCode) {
}

void Connection::onSettings(
    const std::vector<std::pair<SettingParameter, unsigned long>>& settings) {
  for (const auto& setting: settings) {
    TRACE("Setting $0 = $1", setting.first, setting.second);
  }

  generator_.generateSettingsAck();
  endpoint()->wantFlush();
}

void Connection::onSettingsAck() {
}

void Connection::onPushPromise(StreamID sid, StreamID promisedStreamID,
                               HttpRequestInfo&& info) {
}

void Connection::onWindowUpdate(StreamID sid, uint32_t increment) {
}

void Connection::onConnectionError(ErrorCode ec, const std::string& message) {
}

void Connection::onStreamError(StreamID sid, ErrorCode ec,
                               const std::string& message) {
}
// }}}

} // namespace http2
} // namespace http
} // namespace xzero
