// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http1/Connection.h>
#include <xzero/http/http1/Channel.h>
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
#include <xzero/StringUtil.h>
#include <xzero/sysconfig.h>
#include <cassert>
#include <cstdlib>

namespace xzero {
namespace http {
namespace http1 {

#define ERROR(msg...) logError("http.http1.Connection: " msg)

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("http.http1.Connection: " msg)
#else
#define TRACE(msg...) do {} while (0)
#endif

Connection::Connection(TcpEndPoint* endpoint,
                       Executor* executor,
                       HttpHandlerFactory handlerFactory,
                       HttpDateGenerator* dateGenerator,
                       HttpOutputCompressor* outputCompressor,
                       size_t maxRequestUriLength,
                       size_t maxRequestBodyLength,
                       size_t maxRequestCount,
                       Duration maxKeepAlive,
                       size_t inputBufferSize,
                       bool corkStream)
    : TcpConnection(endpoint, executor),
      channel_(std::make_unique<Channel>(
          this, executor, handlerFactory,
          maxRequestUriLength, maxRequestBodyLength,
          dateGenerator, outputCompressor)),
      parser_(Parser::REQUEST, channel_.get()),
      inputBuffer_(inputBufferSize),
      inputOffset_(0),
      writer_(),
      onComplete_(),
      generator_(&writer_),
      maxKeepAlive_(maxKeepAlive),
      requestCount_(0),
      requestMax_(maxRequestCount),
      corkStream_(corkStream) {

  channel_->request()->setRemoteAddress(endpoint->remoteAddress());
  channel_->request()->setLocalAddress(endpoint->localAddress());

  TRACE("{} ctor", this);
}

Connection::~Connection() {
  TRACE("{} dtor", this);
}

void Connection::onOpen(bool dataReady) {
  TRACE("{} onOpen", this);
  TcpConnection::onOpen(dataReady);

  if (dataReady)
    onReadable();
  else
    wantRead();
}

void Connection::abort() {
  TRACE("{} abort()", this);
  channel_->response()->setBytesTransmitted(generator_.bytesTransmitted());
  channel_->responseEnd();

  TRACE("{} abort", this);
  endpoint()->close();
}

void Connection::completed() {
  TRACE("{} completed", this);

  if (channel_->request()->method() != HttpMethod::HEAD &&
      !generator_.isChunked() &&
      generator_.remainingContentLength() > 0)
    throw InvalidState{"Response not fully written but completed() invoked."};

  generator_.generateTrailer(channel_->response()->trailers());

  if (writer_.empty()) {
    onResponseComplete(true);
  } else {
    setCompleter(std::bind(&Connection::onResponseComplete, this,
                           std::placeholders::_1));

    wantWrite();
  }
}

void Connection::upgrade(const std::string& protocol,
                         std::function<void(TcpEndPoint*)> callback) {
  TRACE("upgrade: {}", protocol);
  upgradeCallback_ = callback;
}

void Connection::onResponseComplete(bool succeed) {
  TRACE("{} onResponseComplete({})", this, succeed ? "succeed" : "failure");
  channel_->response()->setBytesTransmitted(generator_.bytesTransmitted());
  channel_->responseEnd();

  if (!succeed) {
    // writing trailer failed. do not attempt to do anything on the wire.
    return;
  }

  if (channel_->response()->status() == HttpStatus::SwitchingProtocols) {
    TRACE("upgrade in action. releasing HTTP/1 connection and invoking callback");
    auto upgrade = upgradeCallback_;
    auto ep = endpoint();

    ep->setConnection(nullptr);
    upgrade(ep);
    TRACE("upgrade complete");

    if (ep->connection())
      ep->connection()->onOpen(false);
    else
      ep->close();

    return;
  }

  if (channel_->isPersistent()) {
    TRACE("{} onResponseComplete: keep-alive was enabled", this);

    // re-use on keep-alive
    channel_->reset();
    generator_.reset();

    endpoint()->setCorking(false);

    if (inputOffset_ < inputBuffer_.size()) {
      // have some request pipelined
      TRACE("{} completed.onComplete: pipelined read", this);
      executor()->execute(std::bind(&Connection::parseFragment, this));
    } else {
      // wait for next request
      TRACE("{} completed.onComplete: keep-alive read", this);
      wantRead();
    }
  } else {
    endpoint()->close();
  }
}

void Connection::send(HttpResponseInfo& responseInfo,
                      const BufferRef& chunk,
                      CompletionHandler onComplete) {
  setCompleter(onComplete, responseInfo.status());

  TRACE("{} send(BufferRef, status={}, persistent={}, chunkSize={})",
        this, responseInfo.status(), channel_->isPersistent() ? "yes" : "no",
        chunk.size());

  patchResponseInfo(responseInfo);

  if (corkStream_)
    endpoint()->setCorking(true);

  generator_.generateResponse(responseInfo, chunk);
  wantWrite();
}

void Connection::send(HttpResponseInfo& responseInfo,
                      Buffer&& chunk,
                      CompletionHandler onComplete) {
  setCompleter(onComplete, responseInfo.status());

  TRACE("{} send(Buffer, status={}, persistent={}, chunkSize={})",
        this, responseInfo.status(), channel_->isPersistent() ? "yes" : "no",
        chunk.size());

  patchResponseInfo(responseInfo);

  if (corkStream_)
    endpoint()->setCorking(true);

  generator_.generateResponse(responseInfo, std::move(chunk));
  wantWrite();
}

void Connection::send(HttpResponseInfo& responseInfo,
                      FileView&& chunk,
                      CompletionHandler onComplete) {
  setCompleter(onComplete, responseInfo.status());

  TRACE("{} send(FileView, status={}, persistent={}, fd={}, chunkSize={})",
        this, responseInfo.status(), channel_->isPersistent() ? "yes" : "no",
        chunk.handle(), chunk.size());

  patchResponseInfo(responseInfo);

  if (corkStream_)
    endpoint()->setCorking(true);

  generator_.generateResponse(responseInfo, std::move(chunk));
  wantWrite();
}

void Connection::setCompleter(CompletionHandler cb) {
  if (cb && onComplete_)
    throw InvalidState{"There is still another completion hook."};

  onComplete_ = std::move(cb);
}

void Connection::setCompleter(CompletionHandler onComplete, HttpStatus status) {
  // make sure, that on ContinueRequest we do actually continue with
  // the prior request

  if (status != HttpStatus::ContinueRequest) {
    setCompleter(onComplete);
  } else {
    setCompleter([this, onComplete](bool s) {
      wantRead();
      if (onComplete) {
        onComplete(s);
      }
    });
  }
}

void Connection::invokeCompleter(bool success) {
  if (onComplete_) {
    TRACE("{} invoking completion callback", this);
    auto callback = std::move(onComplete_);
    onComplete_ = nullptr;
    callback(success);
  }
}

void Connection::patchResponseInfo(HttpResponseInfo& responseInfo) {
  if (static_cast<int>(responseInfo.status()) >= 200) {
    // patch in HTTP transport-layer headers
    if (channel_->isPersistent() && requestCount_ < requestMax_) {
      ++requestCount_;

      char keepAlive[64];
      snprintf(keepAlive, sizeof(keepAlive), "timeout=%llu, max=%zu",
               (unsigned long long) maxKeepAlive_.seconds(),
               requestMax_ - requestCount_);

      responseInfo.headers().append("Connection", "Keep-Alive", ", ");
      responseInfo.headers().push_back("Keep-Alive", keepAlive);
    } else {
      channel_->setPersistent(false);
      responseInfo.headers().append("Connection", "close", ", ");
    }
  }
}

void Connection::send(Buffer&& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  TRACE("{} send(Buffer, chunkSize={})", this, chunk.size());
  generator_.generateBody(std::move(chunk));
  wantWrite();
}

void Connection::send(const BufferRef& chunk,
                      CompletionHandler onComplete) {
  setCompleter(onComplete);
  TRACE("{} send(BufferRef, chunkSize={})", this, chunk.size());
  generator_.generateBody(chunk);
  wantWrite();
}

void Connection::send(FileView&& chunk, CompletionHandler onComplete) {
  setCompleter(onComplete);
  TRACE("{} send(FileView, chunkSize={})", this, chunk.size());
  generator_.generateBody(std::move(chunk));
  wantWrite();
}

void Connection::onReadable() {
  TRACE("{} onReadable", this);

  TRACE("{} onReadable: calling read()", this);
  if (endpoint()->read(&inputBuffer_) == 0) {
    TRACE("{} onReadable: read() returned 0", this);
    // ??? throw RemoteDisconnected{};
    abort();
    return;
  }

  parseFragment();
}

void Connection::parseFragment() {
  try {
    TRACE("parseFragment: calling parseFragment ({} into {})",
          inputOffset_, inputBuffer_.size());
    TRACE("dump: '{}'", inputBuffer_.ref(inputOffset_));
    size_t n = parser_.parseFragment(inputBuffer_.ref(inputOffset_));
    TRACE("parseFragment: called ({} into {}) => {} ({})",
          inputOffset_, inputBuffer_.size(), n,
          parser_.state());
    inputOffset_ += n;

    // on a partial read we must make sure that we wait for more input
    if (parser_.state() != Parser::MESSAGE_BEGIN) {
      wantRead();
    }
  } catch (const BadMessage& e) {
    TRACE("{} parseFragment: BadMessage caught (while in state {}). {}",
          this, to_string(channel_->state()), e.what());

    if (channel_->response()->version() == HttpVersion::UNKNOWN)
      channel_->response()->setVersion(HttpVersion::VERSION_0_9);

    if (channel_->state() == HttpChannelState::READING)
      channel_->setState(HttpChannelState::HANDLING);

    channel_->response()->sendError(e.httpCode(), e.what());
  }
}

void Connection::onWriteable() {
  TRACE("{} onWriteable", this);

  if (channel_->state() != HttpChannelState::SENDING)
    channel_->setState(HttpChannelState::SENDING);

  const bool complete = writer_.flushTo(endpoint());

  if (complete) {
    TRACE("{} onWriteable: completed. ({})",
          this,
          (onComplete_ ? "onComplete cb set" : "onComplete cb not set"));
    channel_->setState(HttpChannelState::HANDLING);

    invokeCompleter(true);
  } else {
    // continue flushing as we still have data pending
    wantWrite();
  }
}

void Connection::onInterestFailure(const std::exception& error) {
  TRACE("{} onInterestFailure({}): {}",
        this, typeid(error).name(), error.what());

  // TODO: improve logging here, as this eats our exception here.
  // e.g. via (factory or connector)->error(error);
  // TODO: logError("Connection", error, "Unhandled exception caught in I/O loop");
  invokeCompleter(false);
  abort();
}

}  // namespace http1
}  // namespace http
}  // namespace xzero
