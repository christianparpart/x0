// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/InetAddress.h>
#include <xzero/executor/Executor.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Duration.h>
#include <utility>
#include <vector>
#include <iosfwd>

namespace xzero {
  class JsonWriter;
}

namespace xzero::http::cluster {

/**
 * Monitors an HTTP endpoint for healthiness.
 */
class HealthMonitor {
 public:
  using HttpClient = xzero::http::client::HttpClient;
  enum class State { Undefined, Offline, Online };
  typedef std::function<void(HealthMonitor*, State)> StateChangeNotify;

  /**
   * Initializes the health monitor.
   *
   * @param executor Executor engine to use for performing I/O and tasks.
   * @param inetAddress Upstream IP:port to connect to
   * @param hostHeader HTTP host header to pass.
   * @param requestPath HTTP request path to use.
   * @param fcgiScriptFilename
   * @param interval The check interval.
   * @param successThreshold Number of consecutive checks to pass until this
   *                         monitor switches from unhealthy to healthy state.
   * @param successCodes HTTP status codes to consider as successful.
   * @param connectTimeout Network connect timeout.
   * @param readTimeout Network read timeout.
   * @param writeTimeout Network write timeout.
   * @param onStateChange Callback to invoke upon state changes.
   */
  HealthMonitor(Executor* executor,
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
                    StateChangeNotify onStateChange);

  ~HealthMonitor();

  const std::string& hostHeader() const noexcept { return hostHeader_; }
  void setHostHeader(const std::string& value) { hostHeader_ = value; }

  const std::string& requestPath() const noexcept { return requestPath_; }
  void setRequestPath(const std::string& value) { requestPath_ = value; }

  unsigned successThreshold() const noexcept { return successThreshold_; }
  void setSuccessThreshold(unsigned value) { successThreshold_ = value; }

  Duration interval() const { return interval_; }
  void setInterval(const Duration& value) { interval_ = value; }

  const std::vector<HttpStatus>& successCodes() const { return successCodes_; };
  void setSuccessCodes(const std::vector<HttpStatus>& value) { successCodes_ = value; }

  Duration connectTimeout() const noexcept { return connectTimeout_; }
  void setConnectTimeout(Duration value) { connectTimeout_ = value; }

  Duration readTimeout() const noexcept { return readTimeout_; }
  void setReadTimeout(Duration value) { readTimeout_ = value; }

  Duration writeTimeout() const noexcept { return writeTimeout_; }
  void setWriteTimeout(Duration value) { writeTimeout_ = value; }

  State state() const { return state_; }
  bool isOnline() const { return state_ == State::Online; }

  void setState(State value);

  void serialize(JsonWriter& json) const;

 private:
  void start();
  void stop();
  void recheck();
  void logSuccess();
  void logFailure();
  void onCheckNow();
  void onFailure(const std::error_code& ec);
  void onResponseReceived(const HttpClient::Response& response);

 private:
  Executor* executor_;
  Executor::HandleRef timerHandle_;
  InetAddress inetAddress_;
  std::string hostHeader_;
  std::string requestPath_;
  std::string fcgiScriptFilename_;
  Duration interval_;
  std::vector<HttpStatus> successCodes_;
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;

  // number of consecutive succeeding responses before marking changing state to *online*.
  unsigned successThreshold_;

  StateChangeNotify onStateChange_;

  State state_;
  size_t totalFailCount_;
  size_t consecutiveSuccessCount_;
  Duration totalOfflineTime_;

  HttpClient client_;
};

std::ostream& operator<<(std::ostream& os, HealthMonitor::State state);

} // namespace xzero::http::cluster
