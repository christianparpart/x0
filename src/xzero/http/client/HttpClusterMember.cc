#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpHealthMonitor.h>

namespace xzero {
namespace http {
namespace client {

HttpClusterMember::HttpClusterMember(
    Executor* executor,
    const std::string& name,
    const IPAddress& ipaddr,
    int port,
    size_t capacity,
    bool enabled,
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
  return HttpClusterSchedulerStatus::Unavailable; // TODO
}

} // namespace client
} // namespace http
} // namespace xzero
