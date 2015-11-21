#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/io/InputStream.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero/JsonWriter.h>

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.client.HttpHealthMonitor", msg)
# define TRACE(msg...) logTrace("http.client.HttpHealthMonitor", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

namespace xzero {

template<>
std::string StringUtil::toString(http::client::HttpHealthMonitor::State state) {
  switch (state) {
    case http::client::HttpHealthMonitor::State::Undefined:
      return "Undefined";
    case http::client::HttpHealthMonitor::State::Offline:
      return "Offline";
    case http::client::HttpHealthMonitor::State::Online:
      return "Online";
  }
}

template<>
std::string StringUtil::toString(http::client::HttpHealthMonitor::Mode mode) {
  switch (mode) {
    case http::client::HttpHealthMonitor::Mode::Paranoid:
      return "Paranoid";
    case http::client::HttpHealthMonitor::Mode::Opportunistic:
      return "Opportunistic";
    case http::client::HttpHealthMonitor::Mode::Lazy:
      return "Lazy";
  }
}

namespace http {
namespace client {

HttpHealthMonitor::HttpHealthMonitor(Executor* executor, const Uri& url)
    : HttpHealthMonitor(executor,
                        url,                      // target url
                        Duration::fromSeconds(2), // interval
                        Mode::Opportunistic,      // monitor mode
                        3,                        // successThreshold
                        {HttpStatus::Ok}) {       // successCodes
}

HttpHealthMonitor::HttpHealthMonitor(Executor* executor,
                                     const Uri& url,
                                     Duration interval,
                                     Mode mode,
                                     unsigned successThreshold,
                                     const std::vector<HttpStatus>& successCodes)
    : executor_(executor),
      timerHandle_(),
      url_(url),
      interval_(interval),
      successCodes_(successCodes),
      mode_(mode),
      successThreshold_(successThreshold),
      onStateChange_(),
      state_(State::Undefined),
      totalFailCount_(0),
      consecutiveSuccessCount_(0),
      totalOfflineTime_(Duration::Zero),
      client_() {
}

HttpHealthMonitor::~HttpHealthMonitor() {
  stop();
}

/**
 * Sets the callback to be invoked on health state changes.
 */
void HttpHealthMonitor::setStateChangeCallback(
    const std::function<void(HttpHealthMonitor*, State)>& callback) {
  onStateChange_ = callback;
}

/**
 * Forces a health-state change.
 */
void HttpHealthMonitor::setState(State value) {
  assert(value != State::Undefined && "Setting state to Undefined is not allowed.");
  if (state_ == value)
    return;

  State oldState = state_;
  state_ = value;

  TRACE("setState: $0", state_);

  if (onStateChange_) {
    onStateChange_(this, oldState);
  }

  if (state_ == State::Offline) {
    executor_->execute(std::bind(&HttpHealthMonitor::start, this));
  }
}

void HttpHealthMonitor::start() {
  timerHandle_ = executor_->executeAfter(
      interval_,
      std::bind(&HttpHealthMonitor::onCheckNow, this));
}

void HttpHealthMonitor::stop() {
  if (timerHandle_) {
    timerHandle_->cancel();
  }
}

void HttpHealthMonitor::recheck() {
  executor_->execute(std::bind(&HttpHealthMonitor::start, this));
}

void HttpHealthMonitor::onCheckNow() {
  // TODO
}

void HttpHealthMonitor::logSuccess() {
  ++consecutiveSuccessCount_;

  if (consecutiveSuccessCount_ >= successThreshold_) {
    TRACE("The successThreshold reached. Going online.");
    setState(State::Online);
  }

  recheck();
}

void HttpHealthMonitor::logFailure() {
  ++totalFailCount_;
  consecutiveSuccessCount_ = 0;

  setState(State::Offline);

  recheck();
}

JsonWriter& operator<<(JsonWriter& json, const HttpHealthMonitor& monitor) {
  json.beginObject()
      .name("mode")(StringUtil::toString(monitor.mode()))
      .name("state")(StringUtil::toString(monitor.state()))
      .name("interval")(monitor.interval().milliseconds())
      .endObject();

  return json;
}

} // namespace client
} // namespace http
} // namespace xzero
