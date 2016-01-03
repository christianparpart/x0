// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/http1/Channel.h>
#include <xzero/http/http1/Connection.h>
#include <xzero/http/http2/Connection.h>
#include <xzero/http/HttpTransport.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/net/EndPoint.h>
#include <xzero/Tokenizer.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace http1 {

#if !defined(NDEBUG)
#define TRACE(fmt...) logTrace("http.http1.Channel", fmt)
#else
#define TRACE(msg...) do {} while (0)
#endif

Channel::Channel(Connection* transport,
                 Executor* executor,
                 const HttpHandler& handler,
                 size_t maxRequestUriLength,
                 size_t maxRequestBodyLength,
                 HttpDateGenerator* dateGenerator,
                 HttpOutputCompressor* outputCompressor)
    : HttpChannel(transport, executor, handler,
                  maxRequestUriLength, maxRequestBodyLength,
                  dateGenerator, outputCompressor),
      persistent_(false),
      connectionOptions_() {
}

Channel::~Channel() {
}

void Channel::reset() {
  connectionOptions_.clear();
  HttpChannel::reset();
}

void Channel::upgrade(const std::string& protocol,
                      std::function<void(EndPoint*)> callback) {
  TRACE("upgrade: $0", protocol);
  Connection* connection = static_cast<Connection*>(transport_);

  connection->upgrade(protocol, callback);

  response_->setStatus(HttpStatus::SwitchingProtocols);
  response_->headers().overwrite("Upgrade", protocol);
  response_->completed();
}

void Channel::onMessageBegin(const BufferRef& method,
                             const BufferRef& entity,
                             HttpVersion version) {
  request_->setBytesReceived(bytesReceived());

  switch (version) {
    case HttpVersion::VERSION_1_1:
      persistent_ = true;
      break;
    case HttpVersion::VERSION_1_0:
    case HttpVersion::VERSION_0_9:
      persistent_ = false;
      break;
    default:
      RAISE(IllegalStateError, "Invalid State. Illegal version passed.");
  }

  HttpChannel::onMessageBegin(method, entity, version);
}

size_t Channel::bytesReceived() const noexcept {
  return static_cast<Connection*>(transport_)->bytesReceived();
}

void Channel::onMessageHeader(const BufferRef& name,
                              const BufferRef& value) {
  request_->setBytesReceived(bytesReceived());

  if (!iequals(name, "Connection")) {
    HttpChannel::onMessageHeader(name, value);
    return;
  }

  std::vector<BufferRef> options = Tokenizer<BufferRef>::tokenize(value, ", ");

  for (const BufferRef& option: options) {
    connectionOptions_.push_back(option.str());

    if (iequals(option, "Keep-Alive")) {
      TRACE("enable keep-alive");
      persistent_ = true;
    } else if (iequals(option, "close")) {
      persistent_ = false;
    }
  }
}

void Channel::onMessageHeaderEnd() {
  request_->setBytesReceived(bytesReceived());

  bool h2c_upgrade = request_->headers().get("Upgrade") == "h2c";
  std::string h2_settings = request_->headers().get("HTTP2-Settings");

  // hide transport-level header fields
  request_->headers().remove("Connection");
  for (const auto& name: connectionOptions_) {
    connectionHeaders_.push_back(name, request_->headers().get(name));
    request_->headers().remove(name);
  }

  if (h2c_upgrade) {
    HttpHandler nextHandler = handler_;
    handler_ = std::bind(&Channel::h2c_switching_protocols, this,
                         h2_settings, nextHandler);
  }

  HttpChannel::onMessageHeaderEnd();
}

void Channel::h2c_switching_protocols(const std::string& settings,
                                      const HttpHandler& nextHandler) {
  printf("http1: attempt to upgrade to h2c.\n");

  // TODO: fully consume body first

  response_->onResponseEnd(std::bind(&Channel::h2c_start, this, settings, nextHandler));

  response_->setStatus(HttpStatus::SwitchingProtocols);
  response_->headers().push_back("Upgrade", "h2c");
  response_->completed();
}

void Channel::h2c_start(const std::string& settings,
                        const HttpHandler& nextHandler) {
  // 1. now exchange http/1.1 connection with h2c connection,
  // 2. negotiate settings
  // 3. start stream #1 with existing request from previous http/1 connection
  printf("http1: NOW, I would switch protocols to h2c: %s\n", settings.c_str());

  Connection* connection = static_cast<Connection*>(transport_);
  EndPoint* endpoint = connection->endpoint();

  endpoint->setConnection<http2::Connection>(
      endpoint,
      executor_,
      nextHandler,
      dateGenerator_,
      outputCompressor_,
      maxRequestBodyLength_,
      connection->maxRequestCount());
}

void Channel::onProtocolError(HttpStatus code, const std::string& message) {
  TRACE("Protocol Error: $0 $1", code, message);

  request_->setBytesReceived(bytesReceived());

  if (!response_->isCommitted()) {
    persistent_ = false;

    if (request_->version() != HttpVersion::UNKNOWN)
      response_->setVersion(request_->version());
    else
      response_->setVersion(HttpVersion::VERSION_0_9);

    response_->sendError(code, message);
  } else {
    transport_->abort();
  }
}

} // namespace http1
} // namespace http
} // namespace xzero
