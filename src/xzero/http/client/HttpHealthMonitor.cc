#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/io/InputStream.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero/JsonWriter.h>
#include <algorithm>

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

HttpHealthMonitor::HttpHealthMonitor(Executor* executor,
                                     const IPAddress& ipaddr,
                                     int port,
                                     const Uri& testUrl,
                                     Duration interval,
                                     Mode mode,
                                     unsigned successThreshold,
                                     const std::vector<HttpStatus>& successCodes,
                                     Duration connectTimeout,
                                     Duration readTimeout,
                                     Duration writeTimeout)
    : executor_(executor),
      timerHandle_(),
      ipaddr_(ipaddr),
      port_(port),
      testUrl_(testUrl),
      interval_(interval),
      successCodes_(successCodes),
      mode_(mode),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
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

void HttpHealthMonitor::logSuccess() {
  DEBUG("logSuccess!");
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

void HttpHealthMonitor::onCheckNow() {
  Future<RefPtr<InetEndPoint>> ep =
      InetEndPoint::connectAsync(ipaddr_, port_, connectTimeout(), executor_);

  ep.onFailure(std::bind(&HttpHealthMonitor::onConnectFailure, this, std::placeholders::_1));
  ep.onSuccess(std::bind(&HttpHealthMonitor::onConnected, this, std::placeholders::_1));
}

void HttpHealthMonitor::onConnectFailure(Status status) {
  DEBUG("Connecting to backend failed. $0", status);
  logFailure();
}

void HttpHealthMonitor::onConnected(const RefPtr<InetEndPoint>& ep) {
  client_ = std::unique_ptr<HttpClient>(new HttpClient(executor_, ep.as<EndPoint>()));

  BufferRef requestBody;

  HttpRequestInfo requestInfo(HttpVersion::VERSION_1_1,
                              HttpMethod::GET,
                              testUrl_.pathAndQuery(),
                              requestBody.size(),
                              { {"Host", testUrl_.hostAndPort()},
                                {"User-Agent", "HttpHealthMonitor"} } );

  client_->send(std::move(requestInfo), requestBody);
  Future<HttpClient*> f = client_->completed();
  f.onFailure(std::bind(&HttpHealthMonitor::onRequestFailure, this, std::placeholders::_1));
  f.onSuccess(std::bind(&HttpHealthMonitor::onResponseReceived, this, std::placeholders::_1));
}

void HttpHealthMonitor::onRequestFailure(Status status) {
  logFailure();
  recheck();
}

void HttpHealthMonitor::onResponseReceived(HttpClient* client) {
  auto i = std::find(successCodes_.begin(),
                     successCodes_.end(),
                     client->responseInfo().status());
  if (i == successCodes_.end()) {
    DEBUG("received bad response status code. $0 ($1)",
          (int) client->responseInfo().status(),
          client->responseInfo().status());
    logFailure();
    return;
  }

  logSuccess();
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
