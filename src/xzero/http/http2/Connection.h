// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/http/http2/Parser.h>
#include <xzero/http/http2/Generator.h>
#include <xzero/http/http2/Stream.h>
#include <xzero/http/http2/FrameListener.h>
#include <xzero/http/HttpHandler.h>
#include <xzero/net/Connection.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/Buffer.h>
#include <memory>
#include <deque>
#include <cstdint>

namespace xzero {

class EndPoint;
class Executor;
class HugeBuffer;

namespace http {

class HttpDateGenerator;
class HttpOutputCompressor;

namespace http2 {

class Connection
  : public ::xzero::Connection,
    public FrameListener {
 public:
  typedef std::vector<std::pair<http2::SettingParameter, unsigned long>>
      Settings;

  Connection(EndPoint* endpoint,
             Executor* executor,
             const HttpHandler& handler,
             HttpDateGenerator* dateGenerator,
             HttpOutputCompressor* outputCompressor,
             size_t maxRequestBodyLength,
             size_t maxRequestCount);

  Connection(EndPoint* endpoint,
             Executor* executor,
             const HttpHandler& handler,
             HttpDateGenerator* dateGenerator,
             HttpOutputCompressor* outputCompressor,
             size_t maxRequestBodyLength,
             size_t maxRequestCount,
             const Settings& settings,
             HttpRequestInfo&& initialRequestInfo,
             HugeBuffer&& initialRequestBody);

  ~Connection();

  void setMaxConcurrentStreams(size_t value);
  size_t maxConcurrentStreams() const;

  Stream* createStream(const HttpRequestInfo& info, StreamID sid);
  void resetStream(Stream* stream);

 protected:
  void parseFragment();

  // Connection overrides
  void onOpen() override;
  void onClose() override;
  void setInputBufferSize(size_t size) override;
  void onFillable() override;
  void onFlushable() override;
  void onInterestFailure(const std::exception& error) override;

  // FrameListener overrides
  void onData(StreamID sid, const BufferRef& data, bool last) override;
  void onRequestBegin(StreamID sid, bool noContent,
                      HttpRequestInfo&& info) override;
  void onPriority(StreamID sid,
                  bool isExclusiveDependency,
                  StreamID streamDependency,
                  unsigned weight) override;
  void onPing(const BufferRef& data) override;
  void onPingAck(const BufferRef& data) override;
  void onGoAway(StreamID sid, ErrorCode errorCode,
                const BufferRef& debugData) override;
  void onResetStream(StreamID sid, ErrorCode errorCode) override;
  void onSettings(
      const std::vector<std::pair<SettingParameter, unsigned long>>&
        settings) override;
  void onSettingsAck() override;
  void onPushPromise(StreamID sid, StreamID promisedStreamID,
                     HttpRequestInfo&& info) override;
  void onWindowUpdate(StreamID sid, uint32_t increment) override;
  void onConnectionError(ErrorCode ec, const std::string& message) override;
  void onStreamError(StreamID sid, ErrorCode ec,
                     const std::string& message) override;

 private:
  // input management
  Buffer inputBuffer_;
  size_t inputOffset_;
  Parser parser_;

  // output management
  EndPointWriter writer_;
  Generator generator_;

  // stream management
  size_t lowestStreamIdLocal_;
  size_t lowestStreamIdRemote_;
  size_t maxStreamIdLocal_;
  size_t maxStreamIdRemote_;
  std::deque<std::unique_ptr<Stream>> streams_;
};

inline size_t Connection::maxConcurrentStreams() const {
  return streams_.size();
}

} // namespace http2
} // namespace http
} // namespace xzero
