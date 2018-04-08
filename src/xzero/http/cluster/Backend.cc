// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/cluster/Backend.h>
#include <xzero/http/cluster/Context.h>
#include <xzero/http/cluster/HealthMonitor.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/io/FileView.h>
#include <xzero/JsonWriter.h>
#include <xzero/logging.h>
#include <fmt/format.h>

namespace xzero::http::cluster {

template<typename... Args> constexpr void TRACE(const char* msg, Args... args) {
#ifndef NDEBUG
  ::xzero::logTrace(std::string("http.cluster.Backend: ") + msg, args...);
#endif
}

Backend::Backend(
    EventListener* eventListener,
    Executor* executor,
    const std::string& name,
    const InetAddress& inetAddress,
    size_t capacity,
    bool enabled,
    bool terminateProtection,
    const std::string& protocol,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    const std::string& healthCheckHostHeader,
    const std::string& healthCheckRequestPath,
    const std::string& healthCheckFcgiScriptFilename,
    Duration healthCheckInterval,
    unsigned healthCheckSuccessThreshold,
    const std::vector<HttpStatus>& healthCheckSuccessCodes)
    : eventListener_(eventListener),
      executor_(executor),
      name_(name),
      inetAddress_(inetAddress),
      capacity_(capacity),
      enabled_(enabled),
      terminateProtection_(terminateProtection),
      protocol_(protocol),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      healthMonitor_(std::make_unique<HealthMonitor>(
          executor,
          inetAddress,
          healthCheckHostHeader,
          healthCheckRequestPath,
          healthCheckFcgiScriptFilename,
          healthCheckInterval,
          healthCheckSuccessThreshold,
          healthCheckSuccessCodes,
          connectTimeout,
          readTimeout,
          writeTimeout,
          std::bind(&EventListener::onHealthChanged, eventListener,
                    this, std::placeholders::_2))) {
}

Backend::~Backend() {
}

void Backend::setCapacity(size_t n) {
  size_t old = capacity_;
  capacity_ = n;
  eventListener_->onCapacityChanged(this, old);
}

SchedulerStatus Backend::tryProcess(Context* cr) {
  if (!isEnabled())
    return SchedulerStatus::Unavailable;

  if (!healthMonitor_->isOnline())
    return SchedulerStatus::Unavailable;

  std::lock_guard<std::mutex> lock(lock_);

  if (capacity_ && load_.current() >= capacity_)
    return SchedulerStatus::Overloaded;

  TRACE("Processing request by backend {} {}", name(), inetAddress_);
  TRACE("tryProcess: with executor: {}", cr->executor->toString());

  ++load_;
  cr->backend = this;

  if (!process(cr)) {
    --load_;
    cr->backend = nullptr;
    healthMonitor()->setState(HealthMonitor::State::Offline);
    return SchedulerStatus::Unavailable;
  }

  return SchedulerStatus::Success;
}

void Backend::release() {
  eventListener_->onProcessingSucceed(this);
}

bool Backend::process(Context* cr) {
  cr->client = std::make_unique<HttpClient>(cr->executor,
                                            cr->backend->inetAddress());
  Future<HttpClient::Response> f = cr->client->send(cr->request);

  f.onFailure(std::bind(&Backend::onFailure, this,
                        cr, std::placeholders::_1));
  f.onSuccess(std::bind(&Backend::onResponseReceived, this, cr,
                        std::placeholders::_1));

  return true;
}

void Backend::onFailure(Context* cr, const std::error_code& ec) {
  --load_;
  healthMonitor()->setState(HealthMonitor::State::Offline);

  cr->backend = nullptr;

  eventListener_->onProcessingFailed(cr);
}

void Backend::onResponseReceived(Context* cr,
                                 const HttpClient::Response& response) {
  --load_;

  auto isConnectionHeader = [](const std::string& name) -> bool {
    static const std::vector<std::string> connectionHeaderFields = {
      "Connection",
      // "Content-Length",  // XXX we want the upper layer to know the
                            //     content-length in advance
      "Close",
      "Keep-Alive",
      "TE",
      "Trailer",
      "Transfer-Encoding",
      "Upgrade",
    };

    for (const auto& test: connectionHeaderFields)
      if (iequals(name, test))
        return true;

    return false;
  };

  cr->onMessageBegin(response.version(),
                     response.status(),
                     BufferRef(response.reason()));

  cr->onMessageHeader("X-Director-Backend", BufferRef{name()});

  for (const HeaderField& field: response.headers()) {
    if (!isConnectionHeader(field.name())) {
      cr->onMessageHeader(BufferRef(field.name()), BufferRef(field.value()));
    }
  }

  cr->onMessageHeaderEnd();
  if (response.content().isBuffered()) {
    cr->onMessageContent(response.content().getBuffer());
  } else {
    cr->onMessageContent(response.content().getFileView());
  }

  cr->onMessageEnd();
}

void Backend::serialize(JsonWriter& json) const {
  json.beginObject()
      .name("name")(name_)
      .name("capacity")(capacity_)
      .name("terminate-protection")(terminateProtection_)
      .name("enabled")(enabled_)
      .name("protocol")(protocol())
      .name("hostname")(inetAddress_.ip().str())
      .name("port")(inetAddress_.port())
      .beginObject("stats")
        .name("load")(load_)
      .endObject()
      .name("health")(*healthMonitor_)
      .endObject();
}

} // namespace xzero::http::cluster

namespace xzero {
  template<>
  JsonWriter& JsonWriter::value(const http::cluster::Backend& member) {
    member.serialize(*this);
    return *this;
  }
} // namespace xzero
