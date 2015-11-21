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
namespace http {
namespace client {

class HttpHealthMonitor {
 public:
  enum class Mode { Paranoid, Opportunistic, Lazy };
  enum class State { Undefined, Offline, Online };

  HttpHealthMonitor(Executor* executor, const Uri& url);

  HttpHealthMonitor(Executor* executor,
                    const Uri& url,
                    Duration interval,
                    Mode mode,
                    unsigned successThreshold,
                    const std::vector<HttpStatus>& successCodes);

  ~HttpHealthMonitor();

  Mode mode() const { return mode_; }
  void setMode(Mode value);

  unsigned successThreshold() const noexcept { return successThreshold_; }
  void setSuccessThreshold(unsigned value) { successThreshold_ = value; }

  void setUrl(const Uri& url);
  const Uri& url() const { return url_; }

  Duration interval() const { return interval_; }
  void setInterval(const Duration& interval);

  const std::vector<HttpStatus>& successCodes() const { return successCodes_; };
  void setSuccessCodes(const std::vector<HttpStatus>& codes);

  void setStateChangeCallback(
      const std::function<void(HttpHealthMonitor*, State)>& callback);

  State state() const { return state_; }
  bool isOnline() const { return state_ == State::Online; }

 private:
  void setState(State value);
  void start();
  void stop();
  void recheck();
  void onCheckNow();
  void logSuccess();
  void logFailure();

 private:
  Executor* executor_;
  Executor::HandleRef timerHandle_;

  Uri url_;
  Duration interval_;
  std::vector<HttpStatus> successCodes_;
  Mode mode_;

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
