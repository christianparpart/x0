// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/client/HttpHealthMonitor.h>
#include <xzero/io/StringInputStream.h>
#include <xzero/io/InputStream.h>
#include <xzero/io/FileUtil.h>
#include <xzero/text/IniFile.h>
#include <xzero/JsonWriter.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
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
# define DEBUG(msg...) logDebug("HttpCluster", msg)
# define TRACE(msg...) logTrace("HttpCluster", msg)
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
                  "healthcheck",              // health check Host header value
                  "/",                        // health check request path
                  "",                         // health check fcgi script filename
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
                         const std::string& healthCheckHostHeader,
                         const std::string& healthCheckRequestPath,
                         const std::string& healthCheckFcgiScriptFilename,
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
      healthCheckHostHeader_(healthCheckHostHeader),
      healthCheckRequestPath_(healthCheckRequestPath),
      healthCheckFcgiScriptFilename_(healthCheckFcgiScriptFilename),
      healthCheckInterval_(healthCheckInterval),
      healthCheckSuccessThreshold_(healthCheckSuccessThreshold),
      healthCheckSuccessCodes_(healthCheckSuccessCodes),
      scheduler_(new HttpClusterScheduler::RoundRobin(&members_)),
      load_(),
      queued_(),
      dropped_() {
  TRACE("ctor(name: $0)", name_);
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
      << "queue-timeout=" << queueTimeout_.milliseconds() << "\n"
      //TODO: << "on-client-abort=" << tos(clientAbortAction_) << "\n"
      << "retry-after=" << retryAfter_.seconds() << "\n"
      << "max-retry-count=" << maxRetryCount_ << "\n"
      << "sticky-offline-mode=" << (stickyOfflineMode_ ? "true" : "false") << "\n"
      << "allow-x-sendfile=" << (allowXSendfile_ ? "true" : "false") << "\n"
      << "enqueue-on-unavailable=" << (enqueueOnUnavailable_ ? "true" : "false") << "\n"
      << "connect-timeout=" << connectTimeout_.milliseconds() << "\n"
      << "read-timeout=" << readTimeout_.milliseconds() << "\n"
      << "write-timeout=" << writeTimeout_.milliseconds() << "\n"
      << "health-check-success-threshold=" << healthCheckSuccessThreshold_ << "\n"
      << "health-check-host-header=" << healthCheckHostHeader_ << "\n"
      << "health-check-request-path=" << healthCheckRequestPath_ << "\n"
      << "health-check-fcgi-script-filename=" << healthCheckFcgiScriptFilename_ << "\n"
      << "scheduler=" << scheduler()->name() << "\n"
      << "\n";

#if defined(ENABLE_DIRECTOR_CACHE)
  out << "[cache]\n"
      << "enabled=" << (objectCache().enabled() ? "true" : "false") << "\n"
      << "deliver-active=" << (objectCache().deliverActive() ? "true" : "false")
      << "\n"
      << "deliver-shadow=" << (objectCache().deliverShadow() ? "true" : "false")
      << "\n"
      << "default-ttl=" << objectCache().defaultTTL().milliseconds() << "\n"
      << "default-shadow-ttl="
      << objectCache().defaultShadowTTL().milliseconds() << "\n"
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
        << "health-check-interval=" << b->healthMonitor()->interval().milliseconds() << "\n"
        << "host=" << b->inetAddress().ip().str() << "\n"
        << "port=" << b->inetAddress().port() << "\n"
        << "\n";
  }
  return out.str();
}

void HttpCluster::setConfiguration(const std::string& text,
                                   const std::string& path) {
  size_t changed = 0;
  std::string value;
  IniFile settings;
  settings.load(text);

  if (settings.contains("director", "enabled")) {
    if (!settings.load("director", "enabled", value)) {
      RAISE(RuntimeError, StringUtil::format(
            "Could not load settings value director.enabled in file '$0'",
            path));
    }
    enabled_ = value == "true";
  } else {
    ++changed;
  }

  if (!settings.load("director", "queue-limit", value)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.queue-limit in file '$0'",
          path));
  }
  queueLimit_ = std::stoll(value);

  if (!settings.load("director", "queue-timeout", value)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.queue-timeout in file '$0'",
          path));
  }
  queueTimeout_ = Duration::fromMilliseconds(std::stoll(value));
  shaper()->rootNode()->setQueueTimeout(queueTimeout_);

  if (!settings.load("director", "retry-after", value)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.retry-after in file '$0'",
          path));
  }
  retryAfter_ = Duration::fromSeconds(std::stoll(value));

  if (!settings.load("director", "connect-timeout", value)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.connect-timeout in file '%s'",
          path));
  }
  connectTimeout_ = Duration::fromMilliseconds(std::stoll(value));

  if (!settings.load("director", "read-timeout", value)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.read-timeout in file '%s'",
          path));
  }
  readTimeout_ = Duration::fromMilliseconds(std::stoll(value));

  if (!settings.load("director", "write-timeout", value)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.write-timeout in file '%s'",
          path));
  }
  writeTimeout_ = Duration::fromMilliseconds(std::stoll(value));

#if 0
  if (!settings.load("director", "on-client-abort", value)) {
    clientAbortAction_ = ClientAbortAction::Close;
    logError("HttpCluster",
             "director: Could not load settings value "
             "director.on-client-abort  in file '$0'. Defaulting to '$1'.",
             path, tos(clientAbortAction_));
    ++changed;
  } else {
    Try<ClientAbortAction> t = parseClientAbortAction(value);
    if (t) {
      clientAbortAction_ = t.get();
    } else {
      clientAbortAction_ = ClientAbortAction::Close;
      logWarning("HttpCluster",
          "director: Could not load settings value director.on-client-abort  "
          "in file '$0'. %s Defaulting to '$1'.",
          path, t.errorMessage(), tos(clientAbortAction_));
      ++changed;
    }
  }
#endif

  if (!settings.load("director", "max-retry-count", value)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.max-retry-count in file '$0'",
          path));
  }
  maxRetryCount_ = std::stoll(value);

  if (!settings.load("director", "sticky-offline-mode", value)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.sticky-offline-mode in file '$0'",
          path));
  }
  stickyOfflineMode_ = value == "true";

  if (!settings.load("director", "allow-x-sendfile", value)) {
    logError("HttpCluster",
             "director: Could not load settings value director.x-sendfile "
             "in file '$0'",
             path);
    allowXSendfile_ = false;
    ++changed;
  } else {
    allowXSendfile_ = value == "true";
  }

  if (!settings.load("director", "enqueue-on-unavailable", value)) {
    logError("HttpCluster",
             "director: Could not load settings value "
             "director.enqueue-on-unavailable in file '$0'",
             path);
    enqueueOnUnavailable_ = false;
    ++changed;
  } else {
    allowXSendfile_ = value == "true";
  }

  if (settings.load("director", "health-check-success-threshold",
                     value)) {
    if (!value.empty()) {
      int i = std::stoi(value);
      if (i != 0) {
        healthCheckSuccessThreshold_ = i;
      } else {
        RAISE(RuntimeError, StringUtil::format(
              "director: Could not load settings value "
              "director.health-check-success-threshold in file '$0'",
              path));
      }
    }
  }

  if (!settings.load("director", "health-check-host-header",
                     healthCheckHostHeader_)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.health-check-host-header in file '$0'",
          path));
  }

  if (!settings.load("director", "health-check-request-path",
                     healthCheckRequestPath_)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Could not load settings value "
          "director.health-check-request-path in file '$0'",
          path));
  }

  if (!settings.load("director", "health-check-fcgi-script-filename",
                     healthCheckFcgiScriptFilename_)) {
    healthCheckFcgiScriptFilename_ = "";
  }

  if (!settings.load("director", "scheduler", value)) {
    logWarning("HttpCluster",
               "director: Could not load configuration value for "
               "director.scheduler. Using default scheduler $0.",
               scheduler()->name());
    changed++;
  } else if (!setScheduler(value)) {
    ; // XXX ("Setting scheduler failed. Unknown scheduler $0", value)
  }

#if defined(ENABLE_DIRECTOR_CACHE)
  if (settings.contains("cache", "enabled")) {
    if (!settings.load("cache", "enabled", value)) {
      RAISE(RuntimeError, StringUtil::format(
            "director: Could not load settings value cache.enabled in file '$0'",
            path));
    }
    objectCache().setEnabled(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "deliver-active")) {
    if (!settings.load("cache", "deliver-active", value)) {
      RAISE(RuntimeError, StringUtil::format(
            "director: Could not load settings value "
            "cache.deliver-active in file '$0'",
            path));
    }
    objectCache().setDeliverActive(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "deliver-shadow")) {
    if (!settings.load("cache", "deliver-shadow", value)) {
      RAISE(RuntimeError, StringUtil::format(
            "director: Could not load settings value "
            "cache.deliver-shadow in file '$0'",
            path));
    }
    objectCache().setDeliverShadow(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "default-ttl")) {
    if (!settings.load("cache", "default-ttl", value)) {
      RAISE(RuntimeError, StringUtil::format(
            "director: Could not load settings value cache.default-ttl "
            "in file '$0'",
            path));
      RAISE(RuntimeError, StringUtil::format(
    }
    objectCache().setDefaultTTL(Duration::fromMilliseconds(stoi(value)));
  } else {
    ++changed;
  }

  if (settings.contains("cache", "default-shadow-ttl")) {
    if (!settings.load("cache", "default-shadow-ttl", value)) {
      RAISE(RuntimeError, StringUtil::format(
            "director: Could not load settings value cache.default-ttl "
            "in file '$0'",
            path));
    }
    objectCache().setDefaultShadowTTL(Duration::fromMilliseconds(stoi(value)));
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

    if (key.find(backendSectionPrefix) == 0) {
      loadBackend(settings, key);
    } else if (key.find(bucketSectionPrefix) == 0) {
      loadBucket(settings, key);
    } else {
      RAISE(RuntimeError, StringUtil::format(
            "director: Invalid configuration section '$0' in file '$1'.",
            key, path));
    }
  }

  setMutable(true);

  if (changed) {
    logNotice("HttpCluster",
              "director: Rewriting configuration, as $0 attribute(s) "
              "changed while loading.",
              changed);
    saveConfiguration();
  }
}

void HttpCluster::loadBackend(const IniFile& settings, const std::string& key) {
  std::string name = key.substr(strlen("backend="));

  TRACE("Cluster $0: loading backend: $1", name_, name);

  // capacity
  std::string capacityStr;
  if (!settings.load(key, "capacity", capacityStr)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Item "
          "'capacity' not found in section '$1'.",
          storagePath_, key));
  }
  size_t capacity = std::stoll(capacityStr);

  // protocol
  std::string protocol;
  if (!settings.load(key, "protocol", protocol)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Item "
          "'protocol' not found in section '$1'.",
          storagePath_, key));
  }

  // enabled
  std::string enabledStr;
  if (!settings.load(key, "enabled", enabledStr)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Item "
          "'enabled' not found in section '$1'.",
          storagePath_, key));
  }
  bool enabled = enabledStr == "true";

  // health-check-interval
  std::string hcIntervalStr;
  if (!settings.load(key, "health-check-interval", hcIntervalStr)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Item "
          "'health-check-interval' not found in section '$1'.",
          storagePath_, key));
  }
  Duration hcInterval = Duration::fromMilliseconds(std::stoll(hcIntervalStr));

  // host
  std::string host;
  if (!settings.load(key, "host", host)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Item "
          "'host' not found in section '$1'.",
          storagePath_, key));
  }

  // port
  std::string portStr;
  if (!settings.load(key, "port", portStr)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Item "
          "'port' not found in section '$1'.",
          storagePath_, key));
  }

  int port = std::atoi(portStr.c_str());
  if (port <= 0) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Invalid "
          "port number '$1' for backend '$2'",
          storagePath_, portStr, name));
  }
  InetAddress addr(host, port);

  bool terminateProtection = false; // TODO: make configurable

  // spawn backend (by protocol)
  addMember(name, addr, capacity, enabled, terminateProtection,
            protocol, hcInterval);
}

void HttpCluster::loadBucket(const IniFile& settings, const std::string& key) {
  std::string name = key.substr(strlen("bucket="));

  std::string rateStr;
  if (!settings.load(key, "rate", rateStr)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Item "
          "'rate' not found in section '$1'.",
          storagePath_, key));
  }

  std::string ceilStr;
  if (!settings.load(key, "ceil", ceilStr)) {
    RAISE(RuntimeError, StringUtil::format(
          "director: Error loading configuration file '$0'. Item "
          "'ceil' not found in section '$1'.",
          storagePath_, key));
  }

  char* nptr = nullptr;
  float rate = strtof(rateStr.c_str(), &nptr);
  float ceil = strtof(ceilStr.c_str(), &nptr);

  TokenShaperError ec = createBucket(name, rate, ceil);
  if (ec != TokenShaperError::Success) {
    static const char* str[] = {
        "Success.",
        "Rate limit overflow.",
        "Ceil limit overflow.",
        "Name conflict.",
        "Invalid child node."
    };
    RAISE(RuntimeError, StringUtil::format(
          "Could not create director's bucket. $0",
          str[(size_t)ec]));
  }
}

bool HttpCluster::setScheduler(const std::string& value) {
  if (value == "rr") {
    setScheduler(std::unique_ptr<HttpClusterScheduler>(new HttpClusterScheduler::RoundRobin(&members_)));
    return true;
  } else if (value == "chance") {
    setScheduler(std::unique_ptr<HttpClusterScheduler>(new HttpClusterScheduler::Chance(&members_)));
    return true;
  } else {
    return false;
  }
}

void HttpCluster::setScheduler(std::unique_ptr<HttpClusterScheduler> scheduler) {
  scheduler_ = std::move(scheduler);
}

void HttpCluster::addMember(const InetAddress& addr) {
  addMember(StringUtil::format("$0", addr),
            addr,
            0,        // capacity (0 = no limit)
            true,     // enabled
            false,    // terminateProtection
            "http",   // protocol
            healthCheckInterval_);
}

void HttpCluster::addMember(const InetAddress& addr, size_t capacity) {
  addMember(StringUtil::format("$0", addr),
            addr,
            capacity,
            true,     // enabled
            false,    // terminateProtection
            "http",   // protocol
            healthCheckInterval_);
}

void HttpCluster::addMember(const std::string& name,
                            const InetAddress& addr,
                            size_t capacity,
                            bool enabled,
                            bool terminateProtection,
                            const std::string& protocol,
                            Duration healthCheckInterval) {
  Executor* const executor = executor_; // TODO: get as function arg for passing: daemon().selectClientScheduler()

  TRACE("addMember: $0 $1", name, addr);

  HttpClusterMember* backend = new HttpClusterMember(
      this,
      executor,
      name,
      addr,
      capacity,
      enabled,
      terminateProtection,
      protocol,
      connectTimeout_,
      readTimeout_,
      writeTimeout_,
      healthCheckHostHeader_,
      healthCheckRequestPath_,
      healthCheckFcgiScriptFilename_,
      healthCheckInterval,
      healthCheckSuccessThreshold_,
      healthCheckSuccessCodes_);

  members_.push_back(backend);
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

void HttpCluster::setHealthCheckHostHeader(const std::string& value) {
  healthCheckHostHeader_ = value;

  for (HttpClusterMember* member: members_) {
    member->healthMonitor()->setHostHeader(value);
  }
}

void HttpCluster::setHealthCheckRequestPath(const std::string& value) {
  healthCheckRequestPath_ = value;

  for (HttpClusterMember* member: members_) {
    member->healthMonitor()->setRequestPath(value);
  }
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
    HttpClusterSchedulerStatus status = scheduler()->schedule(cr);
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
    HttpClusterSchedulerStatus status = scheduler()->schedule(cr);

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
    cr->onMessageHeader(
        BufferRef("Retry-After"),
        BufferRef(StringUtil::toString(retryAfter().seconds())));
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
    return cr;
  }

  return nullptr;
}

void HttpCluster::onTimeout(HttpClusterRequest* cr) {
  --queued_;

  cr->post([this, cr]() {
    Duration diff = MonotonicClock::now() - cr->ctime;
    logInfo("HttpCluster",
            "Queued request timed out ($0). $1 $2",
            diff,
            cr->client.requestInfo().method(),
            cr->client.requestInfo().path());

    serviceUnavailable(cr, HttpStatus::GatewayTimeout);
  });
}

// {{{ EventListener overrides
void HttpCluster::onEnabledChanged(HttpClusterMember* backend) {
  DEBUG("HttpCluster", "onBackendEnabledChanged: $0 $1",
        backend->name(), backend->isEnabled() ? "enabled" : "disabled");
  TRACE("onBackendEnabledChanged: $0 $1",
        backend->name(), backend->isEnabled() ? "enabled" : "disabled");

  if (backend->isEnabled()) {
    shaper()->resize(shaper()->size() + backend->capacity());
  } else {
    shaper()->resize(shaper()->size() - backend->capacity());
  }
}

void HttpCluster::onCapacityChanged(HttpClusterMember* member, size_t old) {
  if (member->isEnabled()) {
    TRACE("onCapacityChanged: member $0 capacity $1", member->name(), member->capacity());
    shaper()->resize(shaper()->size() - old + member->capacity());
  }
}

void HttpCluster::onHealthChanged(HttpClusterMember* backend,
                                  HttpHealthMonitor::State oldState) {
  logInfo("HttpCluster",
          "$0: backend '$1' ($2:$3) is now $4.",
          name(), backend->name(),
          backend->inetAddress().ip(),
          backend->inetAddress().port(),
          backend->healthMonitor()->state());

  if (!backend->isEnabled())
    return;

  if (backend->healthMonitor()->isOnline()) {
    // backend is online and enabled

    TRACE("onHealthChanged: adding capacity to shaper ($0 + $1)",
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
  } else if (oldState == HttpHealthMonitor::State::Online) {
    // backend is offline and enabled
    TRACE("onHealthChanged: removing capacity from shaper ($0 - $1)",
          shaper()->size(), backend->capacity());
    shaper()->resize(shaper()->size() - backend->capacity());
  }
}

void HttpCluster::onProcessingSucceed(HttpClusterMember* member) {
  dequeueTo(member);
}

void HttpCluster::onProcessingFailed(HttpClusterRequest* request) {
  assert(request->bucket != nullptr);
  assert(request->tokens != 0);

  request->bucket->put(1);
  request->tokens = 0;

  reschedule(request);
}
// }}}

void HttpCluster::serialize(JsonWriter& json) const {
  json.beginObject()
      .name("mutable")(isMutable())
      .name("enabled")(isEnabled())
      .name("queue-limit")(queueLimit_)
      .name("queue-timeout")(queueTimeout_.milliseconds())
      // TODO .name("on-client-abort")(tos(clientAbortAction()))
      .name("retry-after")(retryAfter_.seconds())
      .name("max-retry-count")(maxRetryCount_)
      .name("sticky-offline-mode")(stickyOfflineMode_)
      .name("allow-x-sendfile")(allowXSendfile_)
      .name("enqueue-on-unavailable")(enqueueOnUnavailable_)
      .name("connect-timeout")(connectTimeout_.milliseconds())
      .name("read-timeout")(readTimeout_.milliseconds())
      .name("write-timeout")(writeTimeout_.milliseconds())
      .name("health-check-host-header")(healthCheckHostHeader_)
      .name("health-check-request-path")(healthCheckRequestPath_)
      .name("health-check-fcgi-script-name")(healthCheckFcgiScriptFilename_)
      .name("scheduler")(scheduler()->name())
      .beginObject("stats")
        .name("load")(load_)
        .name("queued")(queued_)
        .name("dropped")(dropped_.load())
      .endObject()
#if defined(ENABLE_DIRECTOR_CACHE)
      .name("cache")(*objectCache_)
#endif
      .name("shaper")(shaper_)
      .beginArray("members");

  for (auto& member: members_) {
    json.value(*member);
  }

  json.endArray();
  json.endObject();
}

} // namespace client
} // namespace http

template<>
JsonWriter& JsonWriter::value(const http::client::HttpCluster& cluster) {
  cluster.serialize(*this);
  return *this;
}

} // namespace xzero
