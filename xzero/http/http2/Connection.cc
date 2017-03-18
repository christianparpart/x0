// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/Connection.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/EndPoint.h>
#include <xzero/HugeBuffer.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace http2 {

#define TRACE(msg...) logNotice("http2.Connection", msg)

Connection::Connection(EndPoint* endpoint,
                       Executor* executor,
                       const HttpHandler& handler,
                       HttpDateGenerator* dateGenerator,
                       HttpOutputCompressor* outputCompressor,
                       size_t maxRequestBodyLength,
                       size_t maxRequestCount)
    : xzero::Connection(endpoint, executor),
      inputFlow_(),
      inputBuffer_(1024),
      inputOffset_(0),
      parser_(this),
      maxRequestUriLength_(1024), // TODO
      maxRequestBodyLength_(maxRequestBodyLength),
      maxRequestCount_(maxRequestCount),
      handler_(handler),
      dateGenerator_(dateGenerator),
      outputCompressor_(outputCompressor),
      outputFlow_(),
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

Connection::Connection(EndPoint* endpoint,
                       Executor* executor,
                       const HttpHandler& handler,
                       HttpDateGenerator* dateGenerator,
                       HttpOutputCompressor* outputCompressor,
                       size_t maxRequestBodyLength,
                       size_t maxRequestCount,
                       const Settings& settings,
                       HttpRequestInfo&& initialRequestInfo,
                       HugeBuffer&& initialRequestBody)
    : Connection(endpoint, executor, handler, dateGenerator, outputCompressor,
                 maxRequestBodyLength, maxRequestCount) {

  // TODO: apply settings (without ACK)
  (void) settings;

  // start request with sid = 1
  Stream* stream = createStream(initialRequestInfo, 1);
  stream->appendBody(initialRequestBody.getBuffer());
  stream->closeInput();
  // streams will be automatically be started upon Connection::onOpen()
}

Connection::~Connection() {
  TRACE("dtor");
}

Stream* Connection::createStream(const HttpRequestInfo& info,
                                 StreamID sid) {
  return createStream(info, sid, nullptr, false, 16);
}

Stream* Connection::createStream(const HttpRequestInfo& info,
                                 StreamID sid,
                                 Stream* parentStream,
                                 bool exclusive,
                                 unsigned weight) {
  if (streams_.size() >= maxConcurrentStreams_)
    return nullptr;

  streams_[sid] = std::unique_ptr<Stream>(new Stream(
      sid,
      parentStream,
      exclusive,
      weight,
      this,
      executor(),
      handler_,
      maxRequestUriLength_,
      maxRequestBodyLength_,
      dateGenerator_,
      outputCompressor_));

  Stream* stream = streams_[sid].get();
  HttpChannel* channel = stream->channel();

  channel->onMessageBegin(BufferRef(info.unparsedMethod().data(), info.unparsedMethod().size()),
                          BufferRef(info.unparsedUri().data(), info.unparsedUri().size()),
                          info.version());

  for (const HeaderField& header: info.headers()) {
    channel->onMessageHeader(BufferRef(header.name().data(), header.name().size()),
                             BufferRef(header.value().data(), header.value().size()));
  }

  channel->onMessageHeaderEnd();

  // TODO

  return stream;

}

Stream* Connection::getStreamByID(StreamID sid) {
  auto s = streams_.find(sid);
  return s != streams_.end() ? s->second.get() : nullptr;
}

void Connection::resetStream(Stream* stream, ErrorCode errorCode) {
  StreamID sid = stream->id();

  // TODO: also close any stream < sid

  generator_.generateResetStream(sid, errorCode);
  endpoint()->wantFlush();

  streams_.erase(sid);
}

void Connection::getAllDependentStreams(StreamID parentStreamID,
                                        std::list<Stream*>* output) {
  // TODO:
  // for (auto& stream: streams_) {
  //   if (stream.second->parentStreamID() == parentStreamID) {
  //     output->push_back(stream);
  //   }
  // }
}

// {{{ net::Connection overrides
void Connection::onOpen(bool dataReady) {
  TRACE("onOpen");
  ::xzero::Connection::onOpen(dataReady);

  // send initial server connection preface
  generator_.generateSettings({}); // leave settings at defaults
  wantFlush();

  // TODO
  // for (std::unique_ptr<Stream>& stream: streams_) {
  //   stream->handleRequest();
  // }
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
  Stream* stream = createStream(info, sid);
  if (noContent) {
    stream->closeInput();
  }
}

void Connection::onPriority(StreamID sid,
                bool isExclusiveDependency,
                StreamID streamDependency,
                unsigned weight) {
}

void Connection::onPing(const BufferRef& data) {
  generator_.generatePingAck(data);
  wantFlush();
}

void Connection::onPingAck(const BufferRef& data) {
  // cheers
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
