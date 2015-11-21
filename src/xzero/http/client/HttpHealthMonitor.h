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
#include <xzero/net/IPAddress.h>
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
  enum class Mode { Paranoid, Opportunistic, Lazy };
  enum class State { Undefined, Offline, Online };

  HttpHealthMonitor(Executor* executor,
                    const IPAddress& ipaddr,
                    int port,
                    const Uri& testUrl,
                    Duration interval,
                    Mode mode,
                    unsigned successThreshold,
                    const std::vector<HttpStatus>& successCodes,
                    Duration connectTimeout,
                    Duration readTimeout,
                    Duration writeTimeout);

  ~HttpHealthMonitor();

  Mode mode() const { return mode_; }
  void setMode(Mode value);

  unsigned successThreshold() const noexcept { return successThreshold_; }
  void setSuccessThreshold(unsigned value) { successThreshold_ = value; }

  void setTestUrl(const Uri& url);
  const Uri& testUrl() const { return testUrl_; }

  Duration interval() const { return interval_; }
  void setInterval(const Duration& interval);

  const std::vector<HttpStatus>& successCodes() const { return successCodes_; };
  void setSuccessCodes(const std::vector<HttpStatus>& codes);

  Duration connectTimeout() const noexcept { return connectTimeout_; }
  void setConnectTimeout(Duration value) { connectTimeout_ = value; }

  Duration readTimeout() const noexcept { return readTimeout_; }
  void setReadTimeout(Duration value) { readTimeout_ = value; }

  Duration writeTimeout() const noexcept { return writeTimeout_; }
  void setWriteTimeout(Duration value) { writeTimeout_ = value; }

  void setStateChangeCallback(
      const std::function<void(HttpHealthMonitor*, State)>& callback);

  State state() const { return state_; }
  bool isOnline() const { return state_ == State::Online; }

 private:
  void setState(State value);
  void start();
  void stop();
  void recheck();
  void logSuccess();
  void logFailure();
  void onCheckNow();
  void onConnectFailure(Status status);
  void onConnected(const RefPtr<InetEndPoint>& ep);
  void onRequestFailure(Status status);
  void onResponseReceived(HttpClient* client);

 private:
  Executor* executor_;
  Executor::HandleRef timerHandle_;
  IPAddress ipaddr_;
  int port_;
  Uri testUrl_;
  Duration interval_;
  std::vector<HttpStatus> successCodes_;
  Mode mode_;
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
