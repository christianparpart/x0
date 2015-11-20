#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpHealthCheck.h>
#include <xzero/io/StringInputStream.h>
#include <xzero/io/InputStream.h>
#include <xzero/logging.h>
#include <algorithm>

namespace xzero {
namespace http {
namespace client {

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.client.HttpCluster", msg)
# define TRACE(msg...) logTrace("http.client.HttpCluster", msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

HttpCluster::HttpCluster(const std::string& name, Executor* executor)
    : HttpCluster(name,
                  executor,
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
                         Executor* executor,
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
      shaper_(executor, 0),
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

HttpClusterMember* HttpCluster::findMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const HttpClusterMember& m) -> bool { return m.name() == name; });
  if (i != members_.end()) {
    return &*i;
  } else {
    return nullptr;
  }
}

void HttpCluster::removeMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const HttpClusterMember& m) -> bool { return m.name() == name; });
  if (i != members_.end()) {
    members_.erase(i);
  }
}

void HttpCluster::setExecutor(Executor* executor) {
  shaper()->setExecutor(executor);
}

void HttpCluster::send(HttpClusterRequest* cr) {
  send(cr, rootBucket());
}

void HttpCluster::send(HttpClusterRequest* cr, RequestShaper::Node* bucket) {
  if (!enabled_) {
    serviceUnavailable(cr);
    return;
  }

  cr->bucket = bucket;

  if (cr->bucket->get(1)) {
    HttpClusterSchedulerStatus status = scheduler_->schedule(cr);
    if (status == HttpClusterSchedulerStatus::Success)
      return;

    cr->bucket->put(1);
    cr->tokens = 0;

    if (status == HttpClusterSchedulerStatus::Unavailable &&
        !enqueueOnUnavailable_) {
      serviceUnavailable(cr);
    } else {
      tryEnqueue(cr);
    }
  } else if (cr->bucket->ceil() > 0 || enqueueOnUnavailable_) {
    // there are tokens available (for rent) and we prefer to wait if unavailable
    tryEnqueue(cr);
  } else {
    serviceUnavailable(cr);
  }
}

void HttpCluster::serviceUnavailable(HttpClusterRequest* cr) {
  cr->responseListener->onMessageBegin(
      HttpVersion::VERSION_1_1,
      HttpStatus::ServiceUnavailable,
      BufferRef(StringUtil::toString(HttpStatus::ServiceUnavailable)));

  if (retryAfter() != Duration::Zero) {
    char value[64];
    int vs = snprintf(value, sizeof(value), "%lu", retryAfter().seconds());
    cr->responseListener->onMessageHeader(
        BufferRef("Retry-After"), BufferRef(value, vs));
  }

  cr->responseListener->onMessageHeaderEnd();
  cr->responseListener->onMessageEnd();

  ++dropped_;
}

/**
 * Attempts to enqueue the request, respecting limits.
 *
 * Attempts to enqueue the request on the associated bucket.
 * If enqueuing fails, it instead finishes the request with a 503 (Service
 * Unavailable).
 *
 * @retval true request could be enqueued.
 * @retval false request could not be enqueued. A 503 error response has been
 *               sent out instead.
 */
bool HttpCluster::tryEnqueue(HttpClusterRequest* cr) {
  if (cr->bucket->queued().current() < queueLimit()) {
    cr->backend = nullptr;
    cr->bucket->enqueue(cr);
    ++queued_;

    TRACE("Director $0 [$1] overloaded. Enqueueing request ($2).",
          name(),
          cr->bucket->name(),
          cr->bucket->queued().current());

    return true;
  }

  TRACE("director: '$0' queue limit $1 reached.", name(), queueLimit());

  serviceUnavailable(cr);

  return false;
}

} // namespace client
} // namespace http
} // namespace xzero
