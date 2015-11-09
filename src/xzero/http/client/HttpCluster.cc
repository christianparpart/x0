#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpHealthCheck.h>

namespace xzero {
namespace http {
namespace client {

HttpCluster::HttpCluster()
    : HttpCluster("default",
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

void HttpCluster::send(HttpRequestInfo&& requestInfo,
                       const std::string& requestBody,
                       HttpListener* responseListener) {
}

} // namespace client
} // namespace http
} // namespace xzero
