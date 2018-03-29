// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/fastcgi/Connection.h>
#include <xzero/http/HttpChannel.h>
#include <xzero/http/HttpDateGenerator.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/BadMessage.h>
#include <xzero/net/TcpConnection.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/executor/Executor.h>
#include <xzero/logging.h>
#include <xzero/RuntimeError.h>
#include <xzero/WallClock.h>
#include <xzero/sysconfig.h>
#include <cassert>
#include <cstdlib>

namespace xzero {
namespace http {
namespace fastcgi {

/* TODO
 *
 * - how to handle BadMessage exceptions
 * - test this class for multiplexed requests
 * - test this class for multiplexed responses
 * - test this class for early client aborts
 * - test this class for early server aborts
 */

#ifndef NDEBUG
#define TRACE_CONN(msg...) logTrace("http.fastcgi.Connection: " msg)
#define TRACE_TRANSPORT(msg...) logTrace("http.fastcgi.Transport: " msg)
#define TRACE_CHANNEL(msg...) logTrace("http.fastcgi.Channel: " msg)
#else
#define TRACE_CONN(msg...) do {} while (0)
#define TRACE_TRANSPORT(msg...) do {} while (0)
#define TRACE_CHANNEL(msg...) do {} while (0)
#endif

class HttpFastCgiTransport : public HttpTransport { // {{{
 public:
  HttpFastCgiTransport(Connection* connection,
                       int id, EndPointWriter* writer);
  virtual ~HttpFastCgiTransport();

  void setChannel(HttpChannel* channel) { channel_ = channel; }

  void abort() override;
  void completed() override;
  void send(HttpResponseInfo& responseInfo, const BufferRef& chunk, CompletionHandler onComplete) override;
  void send(HttpResponseInfo& responseInfo, Buffer&& chunk, CompletionHandler onComplete) override;
  void send(HttpResponseInfo& responseInfo, FileView&& chunk, CompletionHandler onComplete) override;
  void send(Buffer&& chunk, CompletionHandler onComplete) override;
  void send(FileView&& chunk, CompletionHandler onComplete) override;
  void send(const BufferRef& chunk, CompletionHandler onComplete) override;

 private:
  void setCompleter(CompletionHandler ch);
  void onResponseComplete(bool success);

 private:
  Connection* connection_;
  HttpChannel* channel_;
  int id_;
  Generator generator_;
  CompletionHandler onComplete_;
};

void HttpFastCgiTransport::setCompleter(CompletionHandler onComplete) {
  if (!onComplete)
    return;

  if (onComplete_) {
    throw InvalidState{"there is still another completion hook."};
  }

  onComplete_ = onComplete;

  connection_->onComplete_.emplace_back([this](bool success) {
    TRACE_TRANSPORT("$0 setCompleter: callback($1)", this, success ? "success" : "failed");
    auto cb = std::move(onComplete_);
    onComplete_ = nullptr;
    cb(success);
  });

  generator_.flushBuffer();
  connection_->wantWrite();
}

HttpFastCgiTransport::HttpFastCgiTransport(Connection* connection,
                                           int id,
                                           EndPointWriter* writer)
    : connection_(connection),
      channel_(nullptr),
      id_(id),
      generator_(id, writer) {
  TRACE_TRANSPORT("$0 ctor", this);
}

HttpFastCgiTransport::~HttpFastCgiTransport() {
  TRACE_TRANSPORT("$0 dtor", this);
}

void HttpFastCgiTransport::abort() { // TODO
  TRACE_TRANSPORT("$0 abort!", this);
  logFatal("NotImplementedError");

  // channel_->response()->setBytesTransmitted(generator_.bytesTransmitted());
  // channel_->responseEnd();
  // endpoint()->close();
}

void HttpFastCgiTransport::completed() {
  TRACE_TRANSPORT("$0 completed()", this);

  if (onComplete_)
    throw InvalidState{"there is still another completion hook."};

  generator_.generateEnd();

  setCompleter(std::bind(&HttpFastCgiTransport::onResponseComplete, this,
                         std::placeholders::_1));
}

void HttpFastCgiTransport::onResponseComplete(bool success) {
  TRACE_TRANSPORT("$0 onResponseComplete($1)", this, success ? "success" : "failure");

  channel_->response()->setBytesTransmitted(generator_.bytesTransmitted());
  channel_->responseEnd();

  connection_->removeChannel(id_);
}

void HttpFastCgiTransport::send(HttpResponseInfo& responseInfo, const BufferRef& chunk, CompletionHandler onComplete) {
  generator_.generateResponse(responseInfo, chunk);
  setCompleter(onComplete);
}

void HttpFastCgiTransport::send(HttpResponseInfo& responseInfo, Buffer&& chunk, CompletionHandler onComplete) {
  generator_.generateResponse(responseInfo, std::move(chunk));
  setCompleter(onComplete);
}

void HttpFastCgiTransport::send(HttpResponseInfo& responseInfo, FileView&& chunk, CompletionHandler onComplete) {
  generator_.generateResponse(responseInfo, std::move(chunk));
  setCompleter(onComplete);
}

void HttpFastCgiTransport::send(Buffer&& chunk, CompletionHandler onComplete) {
  generator_.generateBody(std::move(chunk));
  setCompleter(onComplete);
}

void HttpFastCgiTransport::send(FileView&& chunk, CompletionHandler onComplete) {
  generator_.generateBody(std::move(chunk));
  setCompleter(onComplete);
}

void HttpFastCgiTransport::send(const BufferRef& chunk, CompletionHandler onComplete) {
  generator_.generateBody(chunk);
  setCompleter(onComplete);
}
// }}}
class HttpFastCgiChannel : public HttpChannel { // {{{
 public:
  HttpFastCgiChannel(std::unique_ptr<HttpTransport> transport,
                     Executor* executor,
                     const HttpHandlerFactory& handlerFactory,
                     size_t maxRequestUriLength,
                     size_t maxRequestBodyLength,
                     HttpDateGenerator* dateGenerator,
                     HttpOutputCompressor* outputCompressor);
  ~HttpFastCgiChannel();

 private:
  std::unique_ptr<HttpTransport> ownedTransport_;
};

HttpFastCgiChannel::HttpFastCgiChannel(
    std::unique_ptr<HttpTransport> transport,
    Executor* executor,
    const HttpHandlerFactory& handlerFactory,
    size_t maxRequestUriLength,
    size_t maxRequestBodyLength,
    HttpDateGenerator* dateGenerator,
    HttpOutputCompressor* outputCompressor)
    : HttpChannel{transport.get(),
                  executor,
                  handlerFactory,
                  maxRequestUriLength,
                  maxRequestBodyLength,
                  dateGenerator,
                  outputCompressor},
      ownedTransport_{std::move(transport)} {
}

HttpFastCgiChannel::~HttpFastCgiChannel() {
}
// }}}

Connection::Connection(TcpEndPoint* endpoint,
                       Executor* executor,
                       const HttpHandlerFactory& handlerFactory,
                       HttpDateGenerator* dateGenerator,
                       HttpOutputCompressor* outputCompressor,
                       size_t maxRequestUriLength,
                       size_t maxRequestBodyLength,
                       Duration maxKeepAlive)
    : TcpConnection(endpoint, executor),
      handlerFactory_(handlerFactory),
      maxRequestUriLength_(maxRequestUriLength),
      maxRequestBodyLength_(maxRequestBodyLength),
      dateGenerator_(dateGenerator),
      outputCompressor_(outputCompressor),
      maxKeepAlive_(maxKeepAlive),
      inputBuffer_(),
      inputOffset_(0),
      persistent_(false),
      parser_(
          std::bind(&Connection::onCreateChannel, this, std::placeholders::_1, std::placeholders::_2),
          std::bind(&Connection::onUnknownPacket, this, std::placeholders::_1, std::placeholders::_2),
          std::bind(&Connection::onAbortRequest, this, std::placeholders::_1)),
      channels_(),
      writer_(),
      onComplete_() {
  inputBuffer_.reserve(4096);
  TRACE_CONN("$0 ctor", this);
}

Connection::~Connection() {
  TRACE_CONN("$0 dtor", this);
}

void Connection::onOpen(bool dataReady) {
  TRACE_CONN("$0 onOpen", this);
  TcpConnection::onOpen(dataReady);

  if (dataReady)
    onReadable();
  else
    wantRead();
}

void Connection::onReadable() {
  TRACE_CONN("$0 onReadable", this);

  TRACE_CONN("$0 onReadable: calling read()", this);
  if (endpoint()->read(&inputBuffer_) == 0) {
    TRACE_CONN("$0 onReadable: read() returned 0", this);
    endpoint()->close();
    // throw RemoteDisconnected{};
    return;
  }

  parseFragment();
}

void Connection::parseFragment() {
  TRACE_CONN("parseFragment: calling parseFragment ($0 into $1)",
             inputOffset_, inputBuffer_.size());
  size_t n = parser_.parseFragment(inputBuffer_.ref(inputOffset_));
  TRACE_CONN("parseFragment: called ($0 into $1) => $2",
             inputOffset_, inputBuffer_.size(), n);
  inputOffset_ += n;
}

void Connection::onWriteable() {
  TRACE_CONN("$0 onWriteable", this);

  const bool complete = writer_.flushTo(endpoint());

  if (complete) {
    TRACE_CONN("$0 onWriteable: completed. ($1)",
          this,
          (!onComplete_.empty() ? "onComplete cb set" : "onComplete cb not set"));

    if (!onComplete_.empty()) {
      TRACE_CONN("$0 onWriteable: invoking completion $1 callback(s)", this, onComplete_.size());
      auto callbacks = std::move(onComplete_);
      onComplete_.clear();
      for (const auto& hook: callbacks) {
        TRACE_CONN("$0 onWriteable: invoking one cb", this);
        hook(true);
      }
    }
  } else {
    // continue flushing as we still have data pending
    wantWrite();
  }
}

void Connection::onInterestFailure(const std::exception& error) {
  TRACE_CONN("$0 onInterestFailure($1): $2",
             this, typeid(error).name(), error.what());

  // TODO: improve logging here, as this eats our exception here.
  // e.g. via (factory or connector)->error(error);
  // TODO: logError("fastcgi", error, "unhandled exception received in I/O loop");

  auto callback = std::move(onComplete_);
  onComplete_.clear();

  // notify the callback that we failed doing something wrt. I/O.
  if (!callback.empty()) {
    TRACE_CONN("$0 onInterestFailure: invoking onComplete(false)", this);
    for (const auto& hook: onComplete_) {
      hook(false);
    }
  }

  endpoint()->close();
}

HttpListener* Connection::onCreateChannel(int request, bool keepAlive) {
  TRACE_CONN("$0 onCreateChannel(requestID=$1, keepalive=$2)",
             this, request, keepAlive ? "yes" : "no");
  setPersistent(keepAlive);
  return createChannel(request);
}

void Connection::onUnknownPacket(int request, int record) {
  TRACE_CONN("$0 onUnknownPacket: request=$1, record=$2 $3",
        this, request, record, to_string(static_cast<Type>(record)));
}

void Connection::onAbortRequest(int request) {
  removeChannel(request);
}

HttpChannel* Connection::createChannel(int request) {
  if (channels_.find(request) != channels_.end()) {
    throw InvalidState{"FastCGI channel with ID $0 already present.", request};
  }

  try {
    std::unique_ptr<HttpFastCgiChannel> channel = std::make_unique<HttpFastCgiChannel>(
        std::make_unique<HttpFastCgiTransport>(this, request, &writer_),
        executor(),
        handlerFactory_,
        maxRequestUriLength_,
        maxRequestBodyLength_,
        dateGenerator_,
        outputCompressor_);

    channel->request()->setRemoteAddress(endpoint()->remoteAddress());

    return (channels_[request] = std::move(channel)).get();
  } catch (...) {
    persistent_ = false;
    removeChannel(request);
    throw;
  }
}

void Connection::removeChannel(int request) {
  TRACE_CONN("$0 removeChannel($1) $2",
             this, request, isPersistent() ? "keepalive" : "close");

  auto i = channels_.find(request);
  if (i != channels_.end()) {
    channels_.erase(i);
  }

  parser_.removeStreamState(request);

  if (isPersistent()) {
    wantRead();
  } else if (channels_.empty()) {
    endpoint()->close();
  }
}

void Connection::setPersistent(bool enable) {
  TRACE_CONN("setPersistent($0) (timeout=$1s)",
      enable ? "yes" : "no",
      maxKeepAlive_.seconds());

  if (maxKeepAlive_ != Duration::Zero) {
    persistent_ = enable;
  }
}

} // namespace fastcgi
} // namespace http
} // namespace xzero
