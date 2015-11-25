// This file is part of the "libxzero" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// libxzero is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HttpResponseInfo.h>
#include <xzero/net/InetAddress.h>
#include <xzero/executor/Executor.h>
#include <xzero/CompletionHandler.h>
#include <xzero/Duration.h>
#include <xzero/Uri.h>
#include <xzero/stdtypes.h>
#include <utility>
#include <vector>

namespace xzero {

class InetEndPoint;

namespace http {
namespace client {

class HttpHealthMonitor {
 public:
  enum class State { Undefined, Offline, Online };
  typedef std::function<void(HttpHealthMonitor*, State)> StateChangeNotify;

  HttpHealthMonitor(Executor* executor,
                    const InetAddress& inetAddress,
                    const Uri& testUrl,
                    Duration interval,
                    unsigned successThreshold,
                    const std::vector<HttpStatus>& successCodes,
                    Duration connectTimeout,
                    Duration readTimeout,
                    Duration writeTimeout,
                    StateChangeNotify onStateChange);

  ~HttpHealthMonitor();

  unsigned successThreshold() const noexcept { return successThreshold_; }
  void setSuccessThreshold(unsigned value) { successThreshold_ = value; }

  const Uri& testUrl() const { return testUrl_; }
  void setTestUrl(const Uri& value) { testUrl_ = value; }

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

  /**
   * Sets the callback to be invoked on health state changes.
   */
  void setStateChangeCallback(StateChangeNotify onStateChange);

  State state() const { return state_; }
  bool isOnline() const { return state_ == State::Online; }

  void setState(State value);

 private:
  void start();
  void stop();
  void recheck();
  void logSuccess();
  void logFailure();
  void onCheckNow();
  void onConnectFailure(Status status);
  void onConnected(const RefPtr<EndPoint>& ep);
  void onRequestFailure(Status status);
  void onResponseReceived(HttpClient* client);

 private:
  Executor* executor_;
  Executor::HandleRef timerHandle_;
  InetAddress inetAddress_;
  Uri testUrl_;
  Duration interval_;
  std::vector<HttpStatus> successCodes_;
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;

  // number of consecutive succeeding responses before marking changing state to *online*.
  unsigned successThreshold_;

  std::function<void(HttpHealthMonitor*, State)> onStateChange_;

  State state_;
  size_t totalFailCount_;
  size_t consecutiveSuccessCount_;
  Duration totalOfflineTime_;

  std::unique_ptr<HttpClient> client_;
};

} // namespace client
} // namespace http
} // namespace xzero
