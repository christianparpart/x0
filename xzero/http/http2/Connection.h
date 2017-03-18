// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/http2/Parser.h>
#include <xzero/http/http2/Generator.h>
#include <xzero/http/http2/Stream.h>
#include <xzero/http/http2/Flow.h>
#include <xzero/http/http2/FrameListener.h>
#include <xzero/http/http2/ErrorCode.h>
#include <xzero/http/HttpHandler.h>
#include <xzero/net/Connection.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/Buffer.h>
#include <memory>
#include <unordered_map>
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

  Stream* createStream(const HttpRequestInfo& info,
                       StreamID sid,
                       Stream* parentStream,
                       bool exclusive,
                       unsigned weight);

  Stream* getStreamByID(StreamID sid);
  void resetStream(Stream* stream, ErrorCode errorCode);

  /**
   * Retrieves all streams that currently depend on given parent stream.
   *
   * @param parentStreamID the parent stream ID to retrieves the dependant
   *                       streams for.
   * @param output         all dependant streams will be appended here.
   */
  void getAllDependentStreams(StreamID parentStreamID,
                              std::list<Stream*>* output);

 protected:
  void parseFragment();

  // Connection overrides
  void onOpen(bool dataReady) override;
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
  Flow inputFlow_;
  Buffer inputBuffer_;
  size_t inputOffset_;
  Parser parser_;

  size_t maxRequestUriLength_;
  size_t maxRequestBodyLength_;
  size_t maxRequestCount_;

  HttpHandler handler_;
  HttpDateGenerator* dateGenerator_;
  HttpOutputCompressor* outputCompressor_;

  // output management
  Flow outputFlow_;
  EndPointWriter writer_;
  Generator generator_;

  // stream management
  size_t lowestStreamIdLocal_;
  size_t lowestStreamIdRemote_;
  size_t maxStreamIdLocal_;
  size_t maxStreamIdRemote_;
  size_t maxConcurrentStreams_;
  std::unordered_map<StreamID, std::unique_ptr<Stream>> streams_;
};

inline size_t Connection::maxConcurrentStreams() const {
  return maxConcurrentStreams_;
}

} // namespace http2
} // namespace http
} // namespace xzero
