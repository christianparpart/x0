// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/HttpChannel.h>
#include <xzero/http/HttpTransport.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/http/HttpDateGenerator.h>
#include <xzero/http/HttpOutputCompressor.h>
#include <xzero/http/HttpVersion.h>
#include <xzero/http/BadMessage.h>
#include <xzero/HugeBuffer.h>
#include <xzero/logging.h>
#include <xzero/io/FileView.h>
#include <xzero/io/Filter.h>
#include <xzero/RuntimeError.h>
#include <xzero/sysconfig.h>

namespace xzero {
namespace http {

std::string as_string(HttpChannelState state) {
  switch (state) {
    case HttpChannelState::READING: return "READING";
    case HttpChannelState::HANDLING: return "HANDLING";
    case HttpChannelState::SENDING: return "SENDING";
    default: {
      char buf[128];
      int n = snprintf(buf, sizeof(buf), "<%d>", static_cast<int>(state));
      return std::string(buf, n);
    }
  }
}

HttpChannel::HttpChannel(HttpTransport* transport,
                         Executor* executor,
                         const HttpHandlerFactory& handlerFactory,
                         size_t maxRequestUriLength,
                         size_t maxRequestBodyLength,
                         HttpDateGenerator* dateGenerator,
                         HttpOutputCompressor* outputCompressor)
    : maxRequestUriLength_(maxRequestUriLength),
      maxRequestBodyLength_(maxRequestBodyLength),
      state_(HttpChannelState::READING),
      transport_(transport),
      executor_(executor),
      request_(std::make_unique<HttpRequest>()),
      response_(std::make_unique<HttpResponse>(this)),
      dateGenerator_(dateGenerator),
      outputFilters_(),
      outputCompressor_(outputCompressor),
      handlerFactory_(handlerFactory),
      handler_() {
  //.
}

HttpChannel::~HttpChannel() {
  //.
}

void HttpChannel::reset() {
  setState(HttpChannelState::READING);
  request_->recycle();
  response_->recycle();
  outputFilters_.clear();
}

void HttpChannel::setState(HttpChannelState newState) {
  state_ = newState;
}

void HttpChannel::addOutputFilter(const std::shared_ptr<Filter>& filter) {
  if (response()->isCommitted())
    throw ResponseAlreadyCommitted{"Cannot add output filters."};

  outputFilters_.push_back(filter);
}

void HttpChannel::removeAllOutputFilters() {
  if (response()->isCommitted())
    throw ResponseAlreadyCommitted{"Cannot remove output filters."};

  outputFilters_.clear();
}

void HttpChannel::send(const BufferRef& data, CompletionHandler onComplete) {
  onBeforeSend();

  if (outputFilters_.empty()) {
    if (!response_->isCommitted()) {
      HttpResponseInfo& info = commitInline();
      transport_->send(info, data, onComplete);
    } else {
      transport_->send(data, onComplete);
    }
  } else {
    Buffer filtered;
    Filter::applyFilters(outputFilters_, data, &filtered, false);

    if (!response_->isCommitted()) {
      HttpResponseInfo& info = commitInline();
      transport_->send(info, std::move(filtered), onComplete);
    } else {
      transport_->send(std::move(filtered), onComplete);
    }
  }
}

void HttpChannel::send(Buffer&& data, CompletionHandler onComplete) {
  onBeforeSend();

  if (!outputFilters_.empty()) {
    Buffer output;
    Filter::applyFilters(outputFilters_, data.ref(), &output, false);
    data = std::move(output);
  }

  if (!response_->isCommitted()) {
    HttpResponseInfo& info = commitInline();
    transport_->send(info, std::move(data), onComplete);
  } else {
    transport_->send(std::move(data), onComplete);
  }
}

void HttpChannel::send(FileView&& file, CompletionHandler onComplete) {
  onBeforeSend();

  if (outputFilters_.empty()) {
    if (!response_->isCommitted()) {
      HttpResponseInfo& info = commitInline();
      transport_->send(info, BufferRef(), nullptr);
      transport_->send(std::move(file), onComplete);
      // transport_->send(info, BufferRef(),
      //     std::bind(&HttpTransport::send, transport_, file, onComplete));
    } else {
      transport_->send(std::move(file), onComplete);
    }
  } else {
    Buffer filtered;
    Filter::applyFilters(outputFilters_, file, &filtered, false);

    if (!response_->isCommitted()) {
      HttpResponseInfo& info = commitInline();
      transport_->send(info, std::move(filtered), onComplete);
    } else {
      transport_->send(std::move(filtered), onComplete);
    }
  }
}

void HttpChannel::send(HugeBuffer&& content, CompletionHandler&& completed) {
  if (content.isFile()) {
    send(content.takeFileView(), completed);
  } else {
    send(content.takeBuffer(), completed);
  }
}

void HttpChannel::onBeforeSend() {
  // also accept READING as current state because you
  // might want to sentError right away, due to protocol error at least.

  if (state() != HttpChannelState::HANDLING &&
      state() != HttpChannelState::READING) {
    throw InvalidState{as_string(state()) + ": Creating a new send object not allowed."};
  }

  //XXX setState(HttpChannelState::SENDING);

  // install some last-minute output filters before committing

  if (response_->isCommitted())
    return;

  // for (HttpChannelListener* listener: listeners_)
  //   listener->onBeforeCommit(request(), response());

  if (outputCompressor_)
    outputCompressor_->postProcess(request(), response());
}

HttpResponseInfo& HttpChannel::commitInline() {
  if (!response_->status())
    throw InvalidState{"No HTTP response status set yet."};

  onPostProcess_();

  if (request_->expect100Continue())
    response_->send100Continue(nullptr);

  response_->setCommitted(true);

  HttpResponseInfo& info = response_->info();

  info.setIsHeadResponse(request_->method() == HttpMethod::HEAD);

  if (!info.headers().contains("Server"))
    info.headers().push_back("Server", "xzero/" PACKAGE_VERSION);

  if (!info.headers().contains("Date") &&
      dateGenerator_ &&
      static_cast<int>(response_->status()) >= 200) {
    Buffer date;
    dateGenerator_->fill(&date);
    info.headers().push_back("Date", date.str());
  }

  return info;
}

void HttpChannel::commit(CompletionHandler onComplete) {
  send(BufferRef(), onComplete);
}

void HttpChannel::send100Continue(CompletionHandler onComplete) {
  if (!request()->expect100Continue())
    throw ResponseAlreadyCommitted{"No 100-continue expected."};

  request()->setExpect100Continue(false);

  HttpResponseInfo info(request_->version(), HttpStatus::ContinueRequest,
                        "Continue", false, 0, {}, {});

  transport_->send(info, BufferRef(), onComplete);
}

void HttpChannel::onMessageBegin(const BufferRef& method,
                                 const BufferRef& entity,
                                 HttpVersion version) {
  response_->setVersion(version);
  request_->setVersion(version);
  request_->setMethod(method.str());
  if (!request_->setUri(entity.str())) {
    RAISE_EXCEPTION(BadMessage, HttpStatus::BadRequest);
  }
}

void HttpChannel::onMessageHeader(const BufferRef& name,
                                  const BufferRef& value) {
  request_->headers().push_back(name.str(), value.str());

  if (iequals(name, "Expect") && iequals(value, "100-continue"))
    request_->setExpect100Continue(true);

  // rfc7230, Section 5.4, p2
  if (iequals(name, "Host")) {
    if (request_->host().empty())
      request_->setHost(value.str());
    else {
      setState(HttpChannelState::HANDLING);
      RAISE_HTTP_REASON(BadRequest, "Multiple host headers are illegal.");
    }
  }
}

void HttpChannel::onMessageHeaderEnd() {
  if (state() != HttpChannelState::HANDLING) {
    setState(HttpChannelState::HANDLING);

    // rfc7230, Section 5.4, p2
    if (request_->version() == HttpVersion::VERSION_1_1) {
      if (!request_->headers().contains("Host")) {
        RAISE_HTTP_REASON(BadRequest, "No Host header given.");
      }
    }

    handleRequest();
  }
}

void HttpChannel::handleRequest() {
  if (request_->headers().contains("Content-Length")) {
    size_t n = std::stoi(request_->headers().get("Content-Length"));

    if (n > maxRequestBodyLength_) {
      if (request_->expect100Continue()) {
        request_->setExpect100Continue(false);
        response_->setStatus(HttpStatus::ExpectationFailed);
      } else {
        response_->setStatus(HttpStatus::PayloadTooLarge);
      }
      response_->completed();
      return;
    }
  }

  try {
    handler_ = handlerFactory_(request(), response());
    handler_();
  } catch (std::exception& e) {
    // TODO: reportException(e);
    // TODO logError("HttpChannel", e, "unhandled exception");
    response()->sendError(HttpStatus::InternalServerError, e.what());
  } catch (...) {
    // TODO: reportException(RUNTIME_ERROR("Unhandled unknown exception caught");
    response()->sendError(HttpStatus::InternalServerError, "unhandled unknown exception");
  }
}

void HttpChannel::onMessageContent(const BufferRef& chunk) {
  request_->fillContent(chunk);
}

void HttpChannel::onMessageContent(FileView&& chunk) {
  request_->fillContent(FileUtil::read(chunk));
}

void HttpChannel::onMessageEnd() {
  request_->ready();
}

void HttpChannel::onError(std::error_code ec) {
  if (ec.category() == HttpStatusCategory::get()) {
    response_->sendError(static_cast<HttpStatus>(ec.value()));
  } else {
    response_->sendError(HttpStatus::InternalServerError);
  }
}

void HttpChannel::completed() {
  if (response_->status() == HttpStatus::NoResponse) {
    transport_->abort();
    return;
  }

  if (request_->method() != HttpMethod::HEAD &&
      response_->hasContentLength() &&
      response_->actualContentLength() < response_->contentLength()) {
    throw InvalidState{
          "Attempt to complete() a response before having written the full response body ({} of {}).",
          response_->actualContentLength(),
          response_->contentLength()};
  }

  if (state() != HttpChannelState::HANDLING) {
    throw InvalidState{"HttpChannel.completed invoked but state is not in HANDLING."};
  }

  if (!outputFilters_.empty()) {
    Buffer filtered;
    Filter::applyFilters(outputFilters_, "", &filtered, true);
    transport_->send(std::move(filtered), nullptr);
  } else if (!response_->isCommitted()) {
    if (!response_->hasContentLength() && request_->method() != HttpMethod::HEAD) {
      response_->setContentLength(0);
    }
    HttpResponseInfo& info = commitInline();
    transport_->send(info, BufferRef(), nullptr);
  }

  transport_->completed();
}

void HttpChannel::onPostProcess(std::function<void()> callback) {
  // TODO: ensure we can still add us
  onPostProcess_.connect(std::move(callback));
}

void HttpChannel::onResponseEnd(std::function<void()> callback) {
  onResponseEnd_.connect(std::move(callback));
}

void HttpChannel::responseEnd() {
  // reset handler to potentially destruct local state
  handler_ = std::function<void()>();

  auto cb = std::move(onResponseEnd_);
  onResponseEnd_.clear();
  cb();
}

}  // namespace http
}  // namespace xzero
