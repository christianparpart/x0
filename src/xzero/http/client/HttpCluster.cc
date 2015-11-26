#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/io/StringInputStream.h>
#include <xzero/io/InputStream.h>
#include <xzero/io/FileUtil.h>
#include <xzero/text/IniFile.h>
#include <xzero/logging.h>
#include <algorithm>
#include <sstream>

namespace xzero {
  using http::client::HttpClusterSchedulerStatus;

  template<> std::string StringUtil::toString(HttpClusterSchedulerStatus value) {
    switch (value) {
      case HttpClusterSchedulerStatus::Unavailable: return "Unavailable";
      case HttpClusterSchedulerStatus::Success: return "Success";
      case HttpClusterSchedulerStatus::Overloaded: return "Overloaded";
    }
  }
}

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

HttpCluster::HttpCluster(const std::string& name,
                         const std::string& storagePath,
                         Executor* executor)
    : HttpCluster(name,
                  storagePath,
                  executor,
                  true,                       // enabled
                  false,                      // stickyOfflineMode
                  true,                       // allowXSendfile
                  true,                       // enqueueOnUnavailable
                  1000,                       // queueLimit
                  30_seconds,                 // queueTimeout
                  30_seconds,                 // retryAfter
                  3,                          // maxRetryCount
                  4_seconds,                  // backend connect timeout
                  30_seconds,                 // backend response read timeout
                  8_seconds,                  // backend request write timeout
                  Uri("http://healthcheck/"), // health check test URI
                  4_seconds,                  // health check interval
                  3,                          // health check success threshold
                  {HttpStatus::Ok}) {         // health check success codes
}

HttpCluster::HttpCluster(const std::string& name,
                         const std::string& storagePath,
                         Executor* executor,
                         bool enabled,
                         bool stickyOfflineMode,
                         bool allowXSendfile,
                         bool enqueueOnUnavailable,
                         size_t queueLimit,
                         Duration queueTimeout,
                         Duration retryAfter,
                         size_t maxRetryCount,
                         Duration connectTimeout,
                         Duration readTimeout,
                         Duration writeTimeout,
                         const Uri& healthCheckUri,
                         Duration healthCheckInterval,
                         unsigned healthCheckSuccessThreshold,
                         const std::vector<HttpStatus>& healthCheckSuccessCodes)
    : name_(name),
      enabled_(enabled),
      stickyOfflineMode_(stickyOfflineMode),
      allowXSendfile_(allowXSendfile),
      enqueueOnUnavailable_(enqueueOnUnavailable),
      queueLimit_(queueLimit),
      queueTimeout_(queueTimeout),
      retryAfter_(retryAfter),
      maxRetryCount_(maxRetryCount),
      connectTimeout_(connectTimeout),
      readTimeout_(readTimeout),
      writeTimeout_(writeTimeout),
      executor_(executor),
      storagePath_(storagePath),
      shaper_(executor, 0,
              std::bind(&HttpCluster::onTimeout, this, std::placeholders::_1)),
      members_(),
      healthCheckUri_(healthCheckUri),
      healthCheckInterval_(healthCheckInterval),
      healthCheckSuccessThreshold_(healthCheckSuccessThreshold),
      healthCheckSuccessCodes_(healthCheckSuccessCodes),
      scheduler_(new HttpClusterScheduler::RoundRobin(&members_)),
      load_(),
      queued_(),
      dropped_() {
}

HttpCluster::~HttpCluster() {
}

void HttpCluster::saveConfiguration() {
  FileUtil::write(storagePath_, configuration());
}

std::string HttpCluster::configuration() const {
  std::ostringstream out;

  out << "# vim:syntax=dosini\n"
      << "# !!! DO NOT EDIT !!! THIS FILE IS GENERATED AUTOMATICALLY !!!\n\n"
      << "[director]\n"
      << "enabled=" << (enabled_ ? "true" : "false") << "\n"
      << "queue-limit=" << queueLimit_ << "\n"
      << "queue-timeout=" << queueTimeout_.seconds() << "\n"
      //<< "on-client-abort=" << tos(clientAbortAction_) << "\n"
      << "retry-after=" << retryAfter_.seconds() << "\n"
      << "max-retry-count=" << maxRetryCount_ << "\n"
      << "sticky-offline-mode=" << (stickyOfflineMode_ ? "true" : "false") << "\n"
      << "allow-x-sendfile=" << (allowXSendfile_ ? "true" : "false") << "\n"
      << "enqueue-on-unavailable=" << (enqueueOnUnavailable_ ? "true" : "false") << "\n"
      << "connect-timeout=" << connectTimeout_.seconds() << "\n"
      << "read-timeout=" << readTimeout_.seconds() << "\n"
      << "write-timeout=" << writeTimeout_.seconds() << "\n"
      //<< "health-check-host-header=" << healthCheckHostHeader_ << "\n"
      //<< "health-check-request-path=" << healthCheckRequestPath_ << "\n"
      //<< "health-check-fcgi-script-filename=" << healthCheckFcgiScriptFilename_ << "\n"
      << "scheduler=" << clusterScheduler()->name() << "\n"
      << "\n";

#if defined(ENABLE_DIRECTOR_CACHE)
  out << "[cache]\n"
      << "enabled=" << (objectCache().enabled() ? "true" : "false") << "\n"
      << "deliver-active=" << (objectCache().deliverActive() ? "true" : "false")
      << "\n"
      << "deliver-shadow=" << (objectCache().deliverShadow() ? "true" : "false")
      << "\n"
      << "default-ttl=" << objectCache().defaultTTL().seconds() << "\n"
      << "default-shadow-ttl="
      << objectCache().defaultShadowTTL().seconds() << "\n"
      << "\n";
#endif

  for (auto& bucket : *shaper()->rootNode()) {
    out << "[bucket=" << bucket->name() << "]\n"
        << "rate=" << bucket->rateP() << "\n"
        << "ceil=" << bucket->ceilP() << "\n"
        << "\n";
  }

  for (auto& b: members_) {
    out << "[backend=" << b->name() << "]\n"
        << "capacity=" << b->capacity() << "\n"
        << "enabled=" << (b->isEnabled() ? "true" : "false") << "\n"
        << "protocol=" << b->protocol() << "\n"
        << "health-check-interval=" << b->healthMonitor()->interval().seconds() << "\n"
        << "host=" << b->inetAddress().ip().str() << "\n"
        << "port=" << b->inetAddress().port() << "\n"
        << "\n";
  }
  return out.str();
}

void HttpCluster::setConfiguration(const std::string& text) {
  size_t changed = 0;
  std::string value;
  IniFile settings;
  settings.load(text);

  if (settings.contains("director", "enabled")) {
    if (!settings.load("director", "enabled", value)) {
      worker()->log(Severity::error,
                    "director: Could not load settings value director.enabled "
                    "in file '%s'",
                    path.c_str());
      return false;
    }
    enabled_ = value == "true";
  } else {
    ++changed;
  }

  if (!settings.load("director", "queue-limit", value)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.queue-limit in file '%s'",
                  path.c_str());
    return false;
  }
  queueLimit_ = std::atoll(value.c_str());

  if (!settings.load("director", "queue-timeout", value)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.queue-timeout in file '%s'",
                  path.c_str());
    return false;
  }
  queueTimeout_ = Duration::fromSeconds(std::atoll(value.c_str()));

  if (!settings.load("director", "retry-after", value)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.retry-after in file '%s'",
                  path.c_str());
    return false;
  }
  retryAfter_ = Duration::fromSeconds(std::atoll(value.c_str()));

  if (!settings.load("director", "connect-timeout", value)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.connect-timeout in file '%s'",
                  path.c_str());
    return false;
  }
  connectTimeout_ = Duration::fromSeconds(std::atoll(value.c_str()));

  if (!settings.load("director", "read-timeout", value)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.read-timeout in file '%s'",
                  path.c_str());
    return false;
  }
  readTimeout_ = Duration::fromSeconds(std::atoll(value.c_str()));

  if (!settings.load("director", "write-timeout", value)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.write-timeout in file '%s'",
                  path.c_str());
    return false;
  }
  writeTimeout_ = Duration::fromSeconds(std::atoll(value.c_str()));

  if (!settings.load("director", "on-client-abort", value)) {
    clientAbortAction_ = ClientAbortAction::Close;
    worker()->log(Severity::warn,
                  "director: Could not load settings value "
                  "director.on-client-abort  in file '%s'. Defaulting to '%s'.",
                  path.c_str(), tos(clientAbortAction_).c_str());
    ++changed;
  } else {
    Try<ClientAbortAction> t = parseClientAbortAction(value);
    if (t) {
      clientAbortAction_ = t.get();
    } else {
      clientAbortAction_ = ClientAbortAction::Close;
      worker()->log(
          Severity::warn,
          "director: Could not load settings value director.on-client-abort  "
          "in file '%s'. %s Defaulting to '%s'.",
          path.c_str(), t.errorMessage(), tos(clientAbortAction_).c_str());
      ++changed;
    }
  }

  if (!settings.load("director", "max-retry-count", value)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.max-retry-count in file '%s'",
                  path.c_str());
    return false;
  }
  maxRetryCount_ = std::atoll(value.c_str());

  if (!settings.load("director", "sticky-offline-mode", value)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.sticky-offline-mode in file '%s'",
                  path.c_str());
    return false;
  }
  stickyOfflineMode_ = value == "true";

  if (!settings.load("director", "allow-x-sendfile", value)) {
    worker()->log(Severity::warn,
                  "director: Could not load settings value director.x-sendfile "
                  "in file '%s'",
                  path.c_str());
    allowXSendfile_ = false;
    ++changed;
  } else {
    allowXSendfile_ = value == "true";
  }

  if (!settings.load("director", "enqueue-on-unavailable", value)) {
    worker()->log(Severity::warn,
                  "director: Could not load settings value "
                  "director.enqueue-on-unavailable in file '%s'",
                  path.c_str());
    enqueueOnUnavailable_ = false;
    ++changed;
  } else {
    allowXSendfile_ = value == "true";
  }

  if (!settings.load("director", "health-check-host-header",
                     healthCheckHostHeader_)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.health-check-host-header in file '%s'",
                  path.c_str());
    return false;
  }

  if (!settings.load("director", "health-check-request-path",
                     healthCheckRequestPath_)) {
    worker()->log(Severity::error,
                  "director: Could not load settings value "
                  "director.health-check-request-path in file '%s'",
                  path.c_str());
    return false;
  }

  if (!settings.load("director", "health-check-fcgi-script-filename",
                     healthCheckFcgiScriptFilename_)) {
    healthCheckFcgiScriptFilename_ = "";
  }

  if (!settings.load("director", "scheduler", value)) {
    worker()->log(Severity::warn,
                  "director: Could not load configuration value for "
                  "director.scheduler. Using default scheduler %s.",
                  scheduler().c_str());
    changed++;
  } else if (!setScheduler(value)) {
    worker()->log(Severity::warn,
                  "director: Invalid cluster configuration value %s detected "
                  "for director.scheduler. Using default scheduler %s.",
                  value.c_str(), scheduler().c_str());
    changed++;
  }

#if defined(ENABLE_DIRECTOR_CACHE)
  if (settings.contains("cache", "enabled")) {
    if (!settings.load("cache", "enabled", value)) {
      worker()->log(
          Severity::error,
          "director: Could not load settings value cache.enabled in file '%s'",
          path.c_str());
      return false;
    }
    objectCache().setEnabled(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "deliver-active")) {
    if (!settings.load("cache", "deliver-active", value)) {
      worker()->log(Severity::error,
                    "director: Could not load settings value "
                    "cache.deliver-active in file '%s'",
                    path.c_str());
      return false;
    }
    objectCache().setDeliverActive(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "deliver-shadow")) {
    if (!settings.load("cache", "deliver-shadow", value)) {
      worker()->log(Severity::error,
                    "director: Could not load settings value "
                    "cache.deliver-shadow in file '%s'",
                    path.c_str());
      return false;
    }
    objectCache().setDeliverShadow(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "default-ttl")) {
    if (!settings.load("cache", "default-ttl", value)) {
      worker()->log(Severity::error,
                    "director: Could not load settings value cache.default-ttl "
                    "in file '%s'",
                    path.c_str());
      return false;
    }
    objectCache().setDefaultTTL(Duration::fromSeconds(stoi(value)));
  } else {
    ++changed;
  }

  if (settings.contains("cache", "default-shadow-ttl")) {
    if (!settings.load("cache", "default-shadow-ttl", value)) {
      worker()->log(Severity::error,
                    "director: Could not load settings value cache.default-ttl "
                    "in file '%s'",
                    path.c_str());
      return false;
    }
    objectCache().setDefaultShadowTTL(Duration::fromSeconds(stoi(value)));
  } else {
    ++changed;
  }
#endif

  for (auto& section : settings) {
    static const std::string backendSectionPrefix("backend=");
    static const std::string bucketSectionPrefix("bucket=");

    std::string key = section.first;
    if (key == "director") continue;

    if (key == "cache") continue;

    bool result = false;
    if (key.find(backendSectionPrefix) == 0)
      result = loadBackend(settings, key);
    else if (key.find(bucketSectionPrefix) == 0)
      result = loadBucket(settings, key);
    else {
      worker()->log(
          Severity::error,
          "director: Invalid configuration section '%s' in file '%s'.",
          key.c_str(), path.c_str());
      result = false;
    }

    if (!result) {
      return false;
    }
  }

  setMutable(true);

  if (changed) {
    worker()->log(Severity::notice,
                  "director: Rewriting configuration, as %d attribute(s) "
                  "changed while loading.",
                  changed);
    saveConfiguration();
  }
}

void HttpCluster::addMember(const InetAddress& addr, size_t capacity) {
  addMember(StringUtil::format("$0", addr), addr, capacity, true);
}

void HttpCluster::addMember(const std::string& name,
                            const InetAddress& addr,
                            size_t capacity,
                            bool enabled) {
  const std::string protocol = "http";  // TODO: get as function arg
  const bool terminateProtection = false;
  Executor* const executor = executor_; // TODO: get as function arg for passing: daemon().selectClientScheduler()

  TRACE("addMember: $0 $1", name, addr);

  HttpClusterMember* backend = new HttpClusterMember(
      executor,
      name,
      addr,
      capacity,
      enabled,
      terminateProtection,
      std::bind(&HttpCluster::onBackendEnabledChanged, this,
                std::placeholders::_1),
      std::bind(&HttpCluster::reschedule, this,
                std::placeholders::_1),
      std::bind(&HttpCluster::onMemberReleased, this,
                std::placeholders::_1),
      protocol,
      connectTimeout(),
      readTimeout(),
      writeTimeout(),
      healthCheckUri(),
      healthCheckInterval(),
      healthCheckSuccessThreshold(),
      healthCheckSuccessCodes(),
      std::bind(&HttpCluster::onBackendHealthStateChanged, this,
                std::placeholders::_1,
                std::placeholders::_2));

  members_.push_back(backend);
}

void HttpCluster::onBackendEnabledChanged(HttpClusterMember* backend) {
  TRACE("onBackendEnabledChanged: $0 $1",
        backend->name(), backend->isEnabled() ? "enabled" : "disabled");

  if (backend->isEnabled())
    shaper()->resize(shaper()->size() + backend->capacity());
  else
    shaper()->resize(shaper()->size() - backend->capacity());
}

void HttpCluster::onBackendHealthStateChanged(HttpClusterMember* backend,
                                        HttpHealthMonitor::State oldState) {
  TRACE("onBackendHealthStateChanged: health=$0 -> $1, enabled=$2",
        oldState,
        backend->healthMonitor()->state(),
        backend->isEnabled());

  logInfo("HttpCluster",
          "$0: backend '$1' ($2:$3) is now $4.",
          name(), backend->name(),
          backend->inetAddress().ip(),
          backend->inetAddress().port(),
          backend->healthMonitor()->state());

  if (backend->healthMonitor()->isOnline()) {
    if (!backend->isEnabled())
      return;

    // backend is online and enabled

    TRACE("onBackendHealthStateChanged: adding capacity to shaper ($0 + $1)",
           shaper()->size(), backend->capacity());
    shaper()->resize(shaper()->size() + backend->capacity());

    if (!stickyOfflineMode()) {
      // try delivering a queued request
      dequeueTo(backend);
    } else {
      // disable backend due to sticky-offline mode
      logNotice(
          "HttpCluster",
          "$0: backend '$1' disabled due to sticky offline mode.",
          name(), backend->name());
      backend->setEnabled(false);
    }
  } else if (backend->isEnabled() && oldState == HttpHealthMonitor::State::Online) {
    // backend is offline and enabled
    shaper()->resize(shaper()->size() - backend->capacity());

    TRACE("onBackendHealthStateChanged: removing capacity from shaper ($0 - $1)",
          shaper()->size(), backend->capacity());
  }
}

void HttpCluster::onMemberReleased(HttpClusterMember* backend) {
  dequeueTo(backend);
}

HttpClusterMember* HttpCluster::findMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const HttpClusterMember* m) -> bool { return m->name() == name; });
  if (i != members_.end()) {
    return *i;
  } else {
    return nullptr;
  }
}

void HttpCluster::removeMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const HttpClusterMember* m) -> bool { return m->name() == name; });
  if (i != members_.end()) {
    delete *i;
    members_.erase(i);
  }
}

void HttpCluster::setHealthCheckUri(const Uri& value) {
  healthCheckUri_ = value;

  for (HttpClusterMember* member: members_)
    member->healthMonitor()->setTestUrl(value);
}

void HttpCluster::setHealthCheckInterval(Duration value) {
  healthCheckInterval_ = value;

  for (HttpClusterMember* member: members_)
    member->healthMonitor()->setInterval(value);
}

void HttpCluster::setHealthCheckSuccessThreshold(unsigned value) {
  healthCheckSuccessThreshold_ = value;

  for (HttpClusterMember* member: members_)
    member->healthMonitor()->setSuccessThreshold(value);
}

void HttpCluster::setHealthCheckSuccessCodes(const std::vector<HttpStatus>& value) {
  healthCheckSuccessCodes_ = value;

  for (HttpClusterMember* member: members_)
    member->healthMonitor()->setSuccessCodes(value);
}

void HttpCluster::setExecutor(Executor* executor) {
  executor_ = executor;
  shaper()->setExecutor(executor);
}

TokenShaperError HttpCluster::createBucket(const std::string& name, float rate,
                                            float ceil) {
  return shaper_.createNode(name, rate, ceil);
}

HttpCluster::Bucket* HttpCluster::findBucket(const std::string& name) const {
  return shaper_.findNode(name);
}

bool HttpCluster::eachBucket(std::function<bool(Bucket*)> body) {
  for (auto& node : *shaper_.rootNode())
    if (!body(node))
      return false;

  return true;
}

void HttpCluster::schedule(HttpClusterRequest* cr) {
  schedule(cr, rootBucket());
}

void HttpCluster::schedule(HttpClusterRequest* cr, Bucket* bucket) {
  cr->bucket = bucket;

  if (!enabled_) {
    serviceUnavailable(cr);
    return;
  }

  if (cr->bucket->get(1)) {
    cr->tokens = 1;
    HttpClusterSchedulerStatus status = clusterScheduler()->schedule(cr);
    if (status == HttpClusterSchedulerStatus::Success)
      return;

    cr->bucket->put(1);
    cr->tokens = 0;

    if (status == HttpClusterSchedulerStatus::Unavailable &&
        !enqueueOnUnavailable_) {
      serviceUnavailable(cr);
    } else {
      enqueue(cr);
    }
  } else if (cr->bucket->ceil() > 0 || enqueueOnUnavailable_) {
    // there are tokens available (for rent) and we prefer to wait if unavailable
    enqueue(cr);
  } else {
    serviceUnavailable(cr);
  }
}

void HttpCluster::reschedule(HttpClusterRequest* cr) {
  TRACE("reschedule");

  if (verifyTryCount(cr)) {
    HttpClusterSchedulerStatus status = clusterScheduler()->schedule(cr);

    if (status != HttpClusterSchedulerStatus::Success) {
      enqueue(cr);
    }
  }
}

/**
 * Verifies number of tries, this request has been attempted to be queued, to be
 * in valid range.
 *
 * @retval true tryCount is still below threashold, so further tries are allowed.
 * @retval false tryCount exceeded limit and a 503 client response has been
 *               sent. Dropped-stats have been incremented.
 */
bool HttpCluster::verifyTryCount(HttpClusterRequest* cr) {
  if (cr->tryCount <= maxRetryCount())
    return true;

  TRACE("proxy.cluster %s: request failed %d times.", name().c_str(), cr->tryCount);
  serviceUnavailable(cr);
  return false;
}

void HttpCluster::serviceUnavailable(HttpClusterRequest* cr, HttpStatus status) {
  cr->onMessageBegin(
      HttpVersion::VERSION_1_1,
      status,
      BufferRef(StringUtil::toString(HttpStatus::ServiceUnavailable)));

  // TODO: put into a more generic place where it affects all responses.
  //
  if (cr->bucket) {
    cr->onMessageHeader(BufferRef("Cluster-Bucket"),
                        BufferRef(cr->bucket->name()));
  }

  if (retryAfter() != Duration::Zero) {
    char value[64];
    int vs = snprintf(value, sizeof(value), "%lu", retryAfter().seconds());
    cr->onMessageHeader(
        BufferRef("Retry-After"), BufferRef(value, vs));
  }

  cr->onMessageHeaderEnd();
  cr->onMessageEnd();

  ++dropped_;
}

/**
 * Attempts to enqueue the request, respecting limits.
 *
 * Attempts to enqueue the request on the associated bucket.
 * If enqueuing fails, it instead finishes the request with a 503 (Service
 * Unavailable).
 */
void HttpCluster::enqueue(HttpClusterRequest* cr) {
  if (cr->bucket->queued().current() < queueLimit()) {
    cr->backend = nullptr;
    cr->bucket->enqueue(cr);
    ++queued_;

    DEBUG("HTTP cluster $0 [$1] overloaded. Enqueueing request ($2).",
          name(),
          cr->bucket->name(),
          cr->bucket->queued().current());
  } else {
    DEBUG("director: '$0' queue limit $1 reached.", name(), queueLimit());
    serviceUnavailable(cr);
  }
}

/**
 * Pops an enqueued request from the front of the queue and passes it to the
 * backend for serving.
 *
 * @param backend the backend to pass the dequeued request to.
 */
void HttpCluster::dequeueTo(HttpClusterMember* backend) {
  if (auto cr = dequeue()) {
    cr->post([this, backend, cr]() {
      cr->tokens = 1;
      DEBUG("Dequeueing request to backend $0 @ $1 ($2)",
          backend->name(), name(), queued_.current());
      HttpClusterSchedulerStatus rc = backend->tryProcess(cr);
      if (rc != HttpClusterSchedulerStatus::Success) {
        cr->tokens = 0;
        logError("HttpCluster",
                 "Dequeueing request to backend $0 @ $1 failed. $2",
                 backend->name(), name(), rc);
        reschedule(cr);
      } else {
        // FIXME: really here????
        verifyTryCount(cr);
      }
    });
  } else {
    TRACE("dequeueTo: queue empty.");
  }
}

HttpClusterRequest* HttpCluster::dequeue() {
  if (auto cr = shaper()->dequeue()) {
    --queued_;
    TRACE("Director $0 dequeued request ($1 pending).", name(), queued_.current());
    return cr;
  }
  TRACE("Director $0 dequeue() failed ($1 pending).", name(), queued_.current());

  return nullptr;
}

void HttpCluster::onTimeout(HttpClusterRequest* cr) {
  --queued_;

  cr->post([this, cr]() {
    Duration diff = MonotonicClock::now() - cr->ctime;
    logInfo("HttpCluster",
            "Queued request timed out ($0). $1 $2",
            diff,
            cr->requestInfo.method(),
            cr->requestInfo.path());

    serviceUnavailable(cr, HttpStatus::GatewayTimeout);
  });
}
} // namespace client
} // namespace http
} // namespace xzero
