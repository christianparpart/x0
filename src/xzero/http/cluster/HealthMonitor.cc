// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/cluster/HealthMonitor.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero/JsonWriter.h>
#include <algorithm>

#if 0 // !defined(NDEBUG)
# define DEBUG(msg...) logDebug("http.cluster.HealthMonitor: " msg)
# define TRACE(msg...) logTrace("http.cluster.HealthMonitor: " msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

namespace xzero::http::cluster {

HealthMonitor::HealthMonitor(Executor* executor,
                             const InetAddress& inetAddress,
                             const std::string& hostHeader,
                             const std::string& requestPath,
                             const std::string& fcgiScriptFilename,
                             Duration interval,
                             unsigned successThreshold,
                             const std::vector<HttpStatus>& successCodes,
                             Duration connectTimeout,
                             Duration readTimeout,
                             Duration writeTimeout,
                             StateChangeNotify onStateChange)
    : executor_(executor),
      timerHandle_(),
      inetAddress_(inetAddress),
      hostHeader_(hostHeader),
      requestPath_(requestPath),
      fcgiScriptFilename_(fcgiScriptFilename),
      interval_(interval),
      successCodes_(successCodes),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      successThreshold_(successThreshold),
      onStateChange_(onStateChange),
      state_(State::Undefined),
      totalFailCount_(0),
      consecutiveSuccessCount_(0),
      totalOfflineTime_(Duration::Zero),
      client_(executor, inetAddress,
              connectTimeout, readTimeout, writeTimeout, Duration::Zero) {
  TRACE("ctor: {}", inetAddress);

  start();
}

HealthMonitor::~HealthMonitor() {
  stop();
}

void HealthMonitor::start() {
  onCheckNow();
}

void HealthMonitor::stop() {
  TRACE("stop");
  if (timerHandle_) {
    timerHandle_->cancel();
  }
}

void HealthMonitor::recheck() {
  TRACE("recheck with interval {}", interval_);
  timerHandle_ = executor_->executeAfter(
      interval_,
      std::bind(&HealthMonitor::onCheckNow, this));
}

void HealthMonitor::logSuccess() {
  TRACE("logSuccess!");
  ++consecutiveSuccessCount_;

  if (consecutiveSuccessCount_ >= successThreshold_ &&
      state() != State::Online) {
    TRACE("The successThreshold reached. Going online.");
    setState(State::Online);
  }

  recheck();
}

void HealthMonitor::logFailure() {
  ++totalFailCount_;
  consecutiveSuccessCount_ = 0;
  TRACE("logFailure {}", totalFailCount_);

  setState(State::Offline);

  recheck();
}

/**
 * Forces a health-state change.
 */
void HealthMonitor::setState(State value) {
  assert(value != State::Undefined && "Setting state to Undefined is not allowed.");
  if (state_ == value)
    return;

  TRACE("setState {} -> {}", state_, value);

  State oldState = state_;
  state_ = value;

  if (onStateChange_) {
    onStateChange_(this, oldState);
  }

  // if (state_ == State::Offline) {
  //   executor_->execute(std::bind(&HealthMonitor::start, this));
  // }
}

void HealthMonitor::onCheckNow() {
  TRACE("onCheckNow");

  timerHandle_.reset();

  Future<HttpClient::Response> f = 
      client_.send(HttpRequest(HttpVersion::VERSION_1_1,
                               HttpMethod::GET,
                               requestPath_,
                               {{"Host", hostHeader_},
                                {"User-Agent", "HealthMonitor"}},
                               false,
                               {}));

  f.onSuccess(std::bind(&HealthMonitor::onResponseReceived, this,
                        std::placeholders::_1));
  f.onFailure(std::bind(&HealthMonitor::onFailure, this,
                        std::placeholders::_1));
}

void HealthMonitor::onFailure(const std::error_code& ec) {
  DEBUG("Connecting to backend failed. {}", ec.message());
  logFailure();
}

void HealthMonitor::onResponseReceived(const HttpClient::Response& response) {
  TRACE("onResponseReceived");
  auto i = std::find(successCodes_.begin(),
                     successCodes_.end(),
                     response.status());
  if (i == successCodes_.end()) {
    DEBUG("Received bad response status code. {} {}",
          static_cast<int>(response.status()),
          response.status());
    logFailure();
    return;
  }

  logSuccess();
}

void HealthMonitor::serialize(JsonWriter& json) const {
  json.beginObject()
      .name("state")(fmt::format("{}", state()))
      .name("interval")(interval().milliseconds())
      .endObject();
}

} // namespace xzero::http::cluster

namespace xzero {
  template<>
  JsonWriter& JsonWriter::value(const http::cluster::HealthMonitor& monitor) {
    monitor.serialize(*this);
    return *this;
  }
}
