#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpHealthCheck.h>
#include <xzero/io/StringInputStream.h>
#include <xzero/io/InputStream.h>
#include <algorithm>

namespace xzero {
namespace http {
namespace client {

HttpCluster::HttpCluster()
    : HttpCluster("default") {
}

HttpCluster::HttpCluster(const std::string& name)
    : HttpCluster(name,
                  true,                      // enabled
                  false,                     // stickyOfflineMode
                  true,                      // allowXSendfile
                  true,                      // enqueueOnUnavailable
                  1000,                      // queueLimit
                  Duration::fromSeconds(30), // queueTimeout
                  Duration::fromSeconds(30), // retryAfter
                  3) {                       // maxRetryCount
}

HttpCluster::HttpCluster(const std::string& name,
                         bool enabled,
                         bool stickyOfflineMode,
                         bool allowXSendfile,
                         bool enqueueOnUnavailable,
                         size_t queueLimit,
                         Duration queueTimeout,
                         Duration retryAfter,
                         size_t maxRetryCount)
    : name_(name),
      enabled_(enabled),
      stickyOfflineMode_(stickyOfflineMode),
      allowXSendfile_(allowXSendfile),
      enqueueOnUnavailable_(enqueueOnUnavailable),
      queueLimit_(queueLimit),
      queueTimeout_(queueTimeout),
      retryAfter_(retryAfter),
      maxRetryCount_(maxRetryCount),
      storagePath_("TODO"),
      members_(),
      scheduler_() {
}

HttpCluster::~HttpCluster() {
}

std::string HttpCluster::configuration() const {
  return ""; // TODO
}

void HttpCluster::setConfiguration(const std::string& text) {
  // TODO
}

void HttpCluster::addMember(const IPAddress& ipaddr, int port, size_t capacity) {
  addMember(StringUtil::format("$0:$1", ipaddr, port),
            ipaddr, port, capacity, true);
}

void HttpCluster::addMember(const std::string& name,
                            const IPAddress& ipaddr,
                            int port,
                            size_t capacity,
                            bool enabled) {
  Executor* executor = nullptr;
  std::string protocol = "http";
  std::unique_ptr<HttpHealthCheck> healthCheck = nullptr;

  members_.emplace_back(executor,
                        name,
                        ipaddr,
                        port,
                        capacity,
                        enabled,
                        protocol,
                        std::move(healthCheck));
}

void HttpCluster::removeMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const HttpClusterMember& m) -> bool { return m.name() == name; });
  if (i != members_.end()) {
    members_.erase(i);
  }
}

void HttpCluster::send(const HttpRequestInfo& requestInfo,
                       std::unique_ptr<InputStream> requestBody,
                       HttpListener* responseListener,
                       Executor* executor) {
  // TODO
}

} // namespace client
} // namespace http
} // namespace xzero
