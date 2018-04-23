// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http2/Connection.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/HugeBuffer.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace http2 {

Connection::Connection(TcpEndPoint* endpoint,
                       Executor* executor,
                       const HttpHandlerFactory& handlerFactory,
                       HttpDateGenerator* dateGenerator,
                       HttpOutputCompressor* outputCompressor,
                       size_t maxRequestBodyLength,
                       size_t maxRequestCount)
    : TcpConnection(endpoint, executor),
      inputFlow_(),
      inputBuffer_(1024),
      inputOffset_(0),
      parser_(this),
      maxRequestUriLength_(1024), // TODO
      maxRequestBodyLength_(maxRequestBodyLength),
      maxRequestCount_(maxRequestCount),
      handlerFactory_(handlerFactory),
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
}

Connection::Connection(TcpEndPoint* endpoint,
                       Executor* executor,
                       const HttpHandlerFactory& handlerFactory,
                       HttpDateGenerator* dateGenerator,
                       HttpOutputCompressor* outputCompressor,
                       size_t maxRequestBodyLength,
                       size_t maxRequestCount,
                       const Settings& settings,
                       HttpRequestInfo&& initialRequestInfo,
                       HugeBuffer&& initialRequestBody)
    : Connection(endpoint, executor, handlerFactory, dateGenerator, outputCompressor,
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

  streams_[sid] = std::make_unique<Stream>(
      sid,
      parentStream,
      exclusive,
      weight,
      this,
      executor(),
      handlerFactory_,
      maxRequestUriLength_,
      maxRequestBodyLength_,
      dateGenerator_,
      outputCompressor_);

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
  endpoint()->wantWrite();

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
  TcpConnection::onOpen(dataReady);

  // send initial server connection preface
  generator_.generateSettings({}); // leave settings at defaults
  wantWrite();

  // TODO
  // for (std::unique_ptr<Stream>& stream: streams_) {
  //   stream->handleRequest();
  // }
}

void Connection::onReadable() {
  if (endpoint()->read(&inputBuffer_) == 0) {
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

void Connection::onWriteable() {
  const bool complete = writer_.flushTo(endpoint());

  if (complete) {
    wantRead();
  } else {
    wantWrite();
  }
}

void Connection::onInterestFailure(const std::exception& error) {
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
  wantWrite();
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
  generator_.generateSettingsAck();
  endpoint()->wantWrite();
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
