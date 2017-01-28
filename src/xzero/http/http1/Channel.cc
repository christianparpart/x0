// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
#include <xzero/base64url.h>
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

  if (h2c_upgrade)
    h2cVerifyUpgrade(std::move(h2_settings));

  HttpChannel::onMessageHeaderEnd();
}

void Channel::h2cVerifyUpgrade(std::string&& settingsPayload) {
  TRACE("http1: verify upgrade to h2c");

  Buffer settingsBuffer;
  settingsBuffer.reserve(base64::decodeLength(settingsPayload));
  settingsBuffer.resize(base64url::decode(settingsPayload.begin(),
                                          settingsPayload.end(),
                                          settingsBuffer.data()));

  std::string debugData;
  Http2Settings settings;
  http2::ErrorCode errorCode = http2::Parser::decodeSettings(settingsBuffer,
                                                             &settings,
                                                             &debugData);

  if (errorCode != http2::ErrorCode::NoError) {
    logDebug("http1.Channel", "Upgrade to h2c failed. $0. $1",
             errorCode, debugData);
    return;
  }

  handler_ = std::bind(&Channel::h2cUpgradeHandler, this,
                       handler_, settings);
}

void Channel::h2cUpgradeHandler(const HttpHandler& nextHandler,
                                const Http2Settings& settings) {
  // TODO: fully consume body first

  Connection* connection = static_cast<Connection*>(transport_);

  upgrade("h2c", std::bind(&Channel::h2cUpgrade,
                           settings,
                           std::placeholders::_1,
                           executor_,
                           nextHandler,
                           dateGenerator_,
                           outputCompressor_,
                           maxRequestBodyLength_,
                           connection->maxRequestCount()));
}

void Channel::h2cUpgrade(const Http2Settings& settings,
                         EndPoint* endpoint,
                         Executor* executor,
                         const HttpHandler& handler,
                         HttpDateGenerator* dateGenerator,
                         HttpOutputCompressor* outputCompressor,
                         size_t maxRequestBodyLength,
                         size_t maxRequestCount) {
  TRACE("http1: Upgrading to h2c.");

  // TODO: pass negotiated settings
  // TODO: pass initial request to be served directly

  HttpRequestInfo info;
  HugeBuffer body(16384);

  endpoint->setConnection<http2::Connection>(
      endpoint,
      executor,
      handler,
      dateGenerator,
      outputCompressor,
      maxRequestBodyLength,
      maxRequestCount,
      settings,
      std::move(info),
      std::move(body));
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
