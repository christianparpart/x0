#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpHealthCheck.h>

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
    std::unique_ptr<HttpHealthCheck> healthCheck)
    : name_(name),
      ipaddress_(ipaddr),
      port_(port),
      protocol_(protocol),
      capacity_(capacity),
      healthCheck_(std::move(healthCheck)),
      enabled_(enabled),
      executor_(executor),
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
