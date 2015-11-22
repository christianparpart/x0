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
    std::unique_ptr<HttpHealthMonitor> healthMonitor)
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
      healthMonitor_(std::move(healthMonitor)),
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
