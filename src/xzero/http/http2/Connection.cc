// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <xzero/http/http2/Connection.h>

namespace xzero {
namespace http {
namespace http2 {

Connection::Connection(EndPoint* ep,
                       Executor* executor,
                       const HttpHandler& handler,
                       HttpDateGenerator* dateGenerator,
                       HttpOutputCompressor* outputCompressor,
                       size_t maxRequestBodyLength,
                       size_t maxRequestCount)
    : xzero::Connection(ep, executor),
      parser_(this),
      generator_(nullptr /*TODO dataChain_*/),
      lowestStreamIdLocal_(0),
      lowestStreamIdRemote_(0),
      maxStreamIdLocal_(0),
      maxStreamIdRemote_(0),
      streams_()
{
}

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
}

void Connection::onResetStream(StreamID sid, ErrorCode errorCode) {
}

void Connection::onSettings(
    const std::vector<std::pair<SettingParameter, unsigned long>>& settings) {
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
// {{{ net::Connection overrides
void Connection::onOpen() {
}

void Connection::onClose() {
}

void Connection::setInputBufferSize(size_t size) {
}

void Connection::onFillable() {
}

void Connection::onFlushable() {
}

void Connection::onInterestFailure(const std::exception& error) {
}
// }}}

} // namespace http2
} // namespace http
} // namespace xzero
