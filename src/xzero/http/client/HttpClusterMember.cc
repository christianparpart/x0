#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpHealthMonitor.h>
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
    Executor* executor,
    const std::string& name,
    const IPAddress& ipaddr,
    int port,
    size_t capacity,
    bool enabled,
    bool terminateProtection,
    std::function<void(HttpClusterMember*)> onEnabledChanged,
    const std::string& protocol,
    Duration connectTimeout,
    Duration readTimeout,
    Duration writeTimeout,
    const Uri& healthCheckUri,
    Duration healthCheckInterval,
    unsigned healthCheckSuccessThreshold,
    const std::vector<HttpStatus>& healthCheckSuccessCodes,
    StateChangeNotify onHealthStateChange)
    : executor_(executor),
      name_(name),
      ipaddress_(ipaddr),
      port_(port),
      capacity_(capacity),
      enabled_(enabled),
      terminateProtection_(terminateProtection),
      onEnabledChanged_(onEnabledChanged),
      protocol_(protocol),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      healthMonitor_(new HttpHealthMonitor(
          executor,
          ipaddr,
          port,
          healthCheckUri,
          healthCheckInterval,
          healthCheckSuccessThreshold,
          healthCheckSuccessCodes,
          connectTimeout,
          readTimeout,
          writeTimeout,
          std::bind(onHealthStateChange, this, std::placeholders::_2))),
      clients_() {
}

HttpClusterMember::~HttpClusterMember() {
}

HttpClusterSchedulerStatus HttpClusterMember::tryProcess(HttpClusterRequest* cr) {
  if (!isEnabled())
    return HttpClusterSchedulerStatus::Unavailable;

  if (!healthMonitor_->isOnline())
    return HttpClusterSchedulerStatus::Unavailable;

  std::lock_guard<std::mutex> lock(lock_);

  if (capacity_ && load_.current() >= capacity_)
    return HttpClusterSchedulerStatus::Overloaded;

  TRACE("Processing request by backend $0 $1:$2", name(), ipaddress_, port_);

  //cr->request->responseHeaders.overwrite("X-Director-Backend", name());

  load_++;
  cr->backend = this;

  if (!process(cr)) {
    load_--;
    cr->backend = nullptr;
    healthMonitor()->setState(HttpHealthMonitor::State::Offline);
    return HttpClusterSchedulerStatus::Unavailable;
  }

  return HttpClusterSchedulerStatus::Success;
}

bool HttpClusterMember::process(HttpClusterRequest* cr) {
  Future<RefPtr<EndPoint>> f = endpoints_.acquire();
  f.onSuccess(std::bind(&HttpClusterMember::onConnected, this,
                        cr, std::placeholders::_1));
  f.onFailure(std::bind(&HttpClusterMember::onConnectFailure, this,
                        cr, std::placeholders::_1));
  return true;
}

void HttpClusterMember::onConnectFailure(HttpClusterRequest* cr, Status status) {
  healthMonitor()->setState(HttpHealthMonitor::State::Offline);
  load_--;

  cr->backend = nullptr;
  // ...
}

void HttpClusterMember::onConnected(HttpClusterRequest* cr,
                                    RefPtr<EndPoint> ep) {
}

} // namespace client
} // namespace http
} // namespace xzero
