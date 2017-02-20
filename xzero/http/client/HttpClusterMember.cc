// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/io/FileView.h>
#include <xzero/JsonWriter.h>
#include <xzero/logging.h>

namespace xzero {
namespace http {
namespace client {

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.client.HttpClusterMember", msg)
# define TRACE(msg...) logTrace("http.client.HttpClusterMember", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

HttpClusterMember::HttpClusterMember(
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
      healthMonitor_(new HttpHealthMonitor(
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
                    this, std::placeholders::_2))),
      clients_() {
}

HttpClusterMember::~HttpClusterMember() {
}

void HttpClusterMember::setCapacity(size_t n) {
  size_t old = capacity_;
  capacity_ = n;
  eventListener_->onCapacityChanged(this, old);
}

HttpClusterSchedulerStatus HttpClusterMember::tryProcess(HttpClusterRequest* cr) {
  if (!isEnabled())
    return HttpClusterSchedulerStatus::Unavailable;

  if (!healthMonitor_->isOnline())
    return HttpClusterSchedulerStatus::Unavailable;

  std::lock_guard<std::mutex> lock(lock_);

  if (capacity_ && load_.current() >= capacity_)
    return HttpClusterSchedulerStatus::Overloaded;

  TRACE("Processing request by backend $0 $1", name(), inetAddress_);
  TRACE("tryProcess: with executor: $0", cr->executor);

  //cr->request->responseHeaders.overwrite("X-Director-Backend", name());

  ++load_;
  cr->backend = this;

  if (!process(cr)) {
    --load_;
    cr->backend = nullptr;
    healthMonitor()->setState(HttpHealthMonitor::State::Offline);
    return HttpClusterSchedulerStatus::Unavailable;
  }

  return HttpClusterSchedulerStatus::Success;
}

void HttpClusterMember::release() {
  eventListener_->onProcessingSucceed(this);
}

bool HttpClusterMember::process(HttpClusterRequest* cr) {
  Future<HttpClient*> f = cr->client.sendAsync(inetAddress_,
                                               connectTimeout_,
                                               readTimeout_,
                                               writeTimeout_);

  f.onFailure(std::bind(&HttpClusterMember::onFailure, this,
                        cr, std::placeholders::_1));
  f.onSuccess(std::bind(&HttpClusterMember::onResponseReceived, this, cr));

  return true;
}

void HttpClusterMember::onFailure(HttpClusterRequest* cr, const std::error_code& ec) {
  --load_;
  healthMonitor()->setState(HttpHealthMonitor::State::Offline);

  cr->backend = nullptr;

  eventListener_->onProcessingFailed(cr);
}

void HttpClusterMember::onResponseReceived(HttpClusterRequest* cr) {
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

  cr->onMessageBegin(cr->client.responseInfo().version(),
                     cr->client.responseInfo().status(),
                     BufferRef(cr->client.responseInfo().reason()));

  for (const HeaderField& field: cr->client.responseInfo().headers()) {
    if (!isConnectionHeader(field.name())) {
      cr->onMessageHeader(BufferRef(field.name()), BufferRef(field.value()));
    }
  }

  cr->onMessageHeaderEnd();
  if (cr->client.isResponseBodyBuffered()) {
    cr->onMessageContent(cr->client.responseBody());
  } else {
    cr->onMessageContent(cr->client.takeResponseBody());
  }
  cr->onMessageEnd();
}

void HttpClusterMember::serialize(JsonWriter& json) const {
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

} // namespace client
} // namespace http

template<>
JsonWriter& JsonWriter::value(const http::client::HttpClusterMember& member) {
  member.serialize(*this);
  return *this;
}

} // namespace xzero
