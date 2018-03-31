// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/mock/Transport.h>
#include <xzero/http/HttpHandler.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpChannel.h>
#include <xzero/http/BadMessage.h>
#include <xzero/executor/Executor.h>
#include <xzero/StringUtil.h>
#include <xzero/io/FileView.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>
#include <stdexcept>
#include <system_error>

namespace xzero {
namespace http {
namespace mock {

Transport::Transport(Executor* executor, HttpHandlerFactory handlerFactory)
    : Transport(executor, handlerFactory, 32, 64, nullptr, nullptr) {
}

Transport::Transport(Executor* executor, HttpHandler handler)
    : Transport(executor,
                [handler](auto a, auto b) { return std::bind(handler, a, b); },
                32, 64, nullptr, nullptr) {
}

Transport::Transport(Executor* executor,
                     const HttpHandlerFactory& handlerFactory,
                     size_t maxRequestUriLength,
                     size_t maxRequestBodyLength,
                     HttpDateGenerator* dateGenerator,
                     HttpOutputCompressor* outputCompressor)
    : executor_(executor),
      handlerFactory_(handlerFactory),
      maxRequestUriLength_(maxRequestUriLength),
      maxRequestBodyLength_(maxRequestBodyLength),
      dateGenerator_(dateGenerator),
      outputCompressor_(outputCompressor),
      isAborted_(false),
      isCompleted_(false),
      channel_(),
      responseChunked_(false),
      responseInfo_(),
      responseBody_() {
}

Transport::~Transport() {
}

void Transport::run(HttpVersion version, const std::string& method,
                        const std::string& entity,
                        const HeaderFieldList& headers,
                        const std::string& body) {
  isCompleted_ = false;
  isAborted_ = false;
  responseInfo_.reset();
  responseBody_.clear();

  channel_ = std::make_unique<HttpChannel>(this, executor_, handlerFactory_,
                                           maxRequestUriLength_,
                                           maxRequestBodyLength_,
                                           dateGenerator_,
                                           outputCompressor_);

  try {
    channel_->onMessageBegin(BufferRef(method), BufferRef(entity), version);

    for (const HeaderField& header: headers) {
      channel_->onMessageHeader(BufferRef(header.name()),
                                BufferRef(header.value()));
    }

    channel_->onMessageHeaderEnd();
    channel_->onMessageContent(BufferRef(body.data(), body.size()));
    channel_->onMessageEnd();
  } catch (const BadMessage& e) {
    channel_->response()->sendError(e.httpCode(), e.what());
  } catch (const RuntimeError& e) {
    channel_->response()->sendError(HttpStatus::InternalServerError, e.what());
  }
}

void Transport::abort() {
  isAborted_ = true;
}

void Transport::completed() {
  isCompleted_ = true;

  responseInfo_.setTrailers(channel_->response()->trailers());
}

void Transport::setResponseInfo(const HttpResponseInfo& info) {
  responseInfo_ = info;
  responseChunked_ = !info.hasContentLength() || info.hasTrailers();

  if (!isContentForbidden(info.status())) {
    if (responseChunked_) {
      responseInfo_.headers().push_back("Transfer-Encoding", "chunked");
    } else {
      responseInfo_.headers().push_back("Content-Length", xzero::to_string(info.contentLength()));
    }
  }

  logTrace("mock.Transport: HTTP/{} {} {}",
           responseInfo_.version(),
           (int)responseInfo_.status(),
           responseInfo_.status());

  for (const auto& header: responseInfo_.headers()) {
    logTrace("mock.Transport: responseHeader[\"{}\"]: \"{}\"", header.name(), header.value());
  }
}

void Transport::send(HttpResponseInfo& responseInfo,
                     const BufferRef& chunk,
                     CompletionHandler onComplete) {
  setResponseInfo(responseInfo);

  responseBody_ += chunk;

  if (onComplete) {
    executor()->execute([onComplete]() {
      onComplete(true);
    });
  }
}

void Transport::send(HttpResponseInfo& responseInfo,
                     Buffer&& chunk,
                     CompletionHandler onComplete) {
  setResponseInfo(responseInfo);
  responseBody_ += chunk;

  if (onComplete) {
    executor()->execute([onComplete]() {
      onComplete(true);
    });
  }
}

void Transport::send(HttpResponseInfo& responseInfo,
                     FileView&& chunk,
                     CompletionHandler onComplete) {
  setResponseInfo(responseInfo);

  chunk.read(&responseBody_);

  if (onComplete) {
    executor()->execute([onComplete]() {
      onComplete(true);
    });
  }
}

void Transport::send(const BufferRef& chunk, CompletionHandler onComplete) {
  responseBody_ += chunk;

  if (onComplete) {
    executor()->execute([onComplete]() {
      onComplete(true);
    });
  }
}

void Transport::send(Buffer&& chunk, CompletionHandler onComplete) {
  responseBody_ += chunk;

  if (onComplete) {
    executor()->execute([onComplete]() {
      onComplete(true);
    });
  }
}

void Transport::send(FileView&& chunk, CompletionHandler onComplete) {
  chunk.read(&responseBody_);

  if (onComplete) {
    executor()->execute([onComplete]() {
      onComplete(true);
    });
  }
}

} // namespace mock
} // namespace http
} // namespace xzero
