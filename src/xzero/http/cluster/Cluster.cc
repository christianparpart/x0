// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/cluster/Cluster.h>
#include <xzero/http/cluster/Context.h>
#include <xzero/http/cluster/Backend.h>
#include <xzero/http/cluster/HealthMonitor.h>
#include <xzero/io/FileUtil.h>
#include <xzero/text/IniFile.h>
#include <xzero/JsonWriter.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <algorithm>
#include <sstream>

namespace xzero::http::cluster {

#ifndef NDEBUG
# define DEBUG(msg...) logDebug("http.cluster.Cluster: " msg)
# define TRACE(msg...) logTrace("http.cluster.Cluster: " msg)
#else
# define DEBUG(msg...) do {} while (0)
# define TRACE(msg...) do {} while (0)
#endif

std::ostream& operator<<(std::ostream& os, SchedulerStatus value) {
  switch (value) {
    case SchedulerStatus::Unavailable: return os << "Unavailable";
    case SchedulerStatus::Success: return os << "Success";
    case SchedulerStatus::Overloaded: return os << "Overloaded";
    default:
      logFatal("Unknown SchedulerStatus value.");
  }
}

Cluster::Cluster(const std::string& name,
                 const std::string& storagePath,
                 Executor* executor)
    : Cluster(name,
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
              10_seconds,                 // health check interval
              3,                          // health check success threshold
              {HttpStatus::Ok,            // health check success codes
               HttpStatus::NoContent,
               HttpStatus::MovedPermanently,
               HttpStatus::MovedTemporarily,
               HttpStatus::TemporaryRedirect,
               HttpStatus::PermanentRedirect}) {
}

Cluster::Cluster(const std::string& name,
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
              std::bind(&Cluster::onTimeout, this, std::placeholders::_1)),
      members_(),
      healthCheckHostHeader_(healthCheckHostHeader),
      healthCheckRequestPath_(healthCheckRequestPath),
      healthCheckFcgiScriptFilename_(healthCheckFcgiScriptFilename),
      healthCheckInterval_(healthCheckInterval),
      healthCheckSuccessThreshold_(healthCheckSuccessThreshold),
      healthCheckSuccessCodes_(healthCheckSuccessCodes),
      scheduler_(std::make_unique<Scheduler::RoundRobin>(&members_)),
      load_(),
      queued_(),
      dropped_() {
  TRACE("ctor(name: {})", name_);
}

Cluster::~Cluster() {
}

void Cluster::saveConfiguration() {
  FileUtil::write(storagePath_, configuration());
}

std::string Cluster::configuration() const {
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

  for (const auto& bucket : *shaper()->rootNode()) {
    out << "[bucket=" << bucket->name() << "]\n"
        << "rate=" << bucket->rateP() << "\n"
        << "ceil=" << bucket->ceilP() << "\n"
        << "\n";
  }

  for (const auto& b: members_) {
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

void Cluster::setConfiguration(const std::string& text,
                               const std::string& path) {
  size_t changed = 0;
  std::string value;
  IniFile settings;
  settings.load(text);

  if (settings.contains("director", "enabled")) {
    if (!settings.load("director", "enabled", value)) {
      throw std::runtime_error{fmt::format(
            "Could not load settings value director.enabled in file '{}'",
            path)};
    }
    enabled_ = value == "true";
  } else {
    ++changed;
  }

  if (!settings.load("director", "queue-limit", value)) {
    throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "director.queue-limit in file '{}'",
          path)};
  }
  queueLimit_ = std::stoll(value);

  if (!settings.load("director", "queue-timeout", value)) {
    throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "director.queue-timeout in file '{}'",
          path)};
  }
  queueTimeout_ = Duration::fromMilliseconds(std::stoll(value));
  shaper()->rootNode()->setQueueTimeout(queueTimeout_);

  if (!settings.load("director", "retry-after", value)) {
    throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "director.retry-after in file '{}'",
          path)};
  }
  retryAfter_ = Duration::fromSeconds(std::stoll(value));

  if (!settings.load("director", "connect-timeout", value)) {
    throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "director.connect-timeout in file '%s'",
          path)};
  }
  connectTimeout_ = Duration::fromMilliseconds(std::stoll(value));

  if (!settings.load("director", "read-timeout", value)) {
    throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "director.read-timeout in file '%s'",
          path)};
  }
  readTimeout_ = Duration::fromMilliseconds(std::stoll(value));

  if (!settings.load("director", "write-timeout", value)) {
    throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "director.write-timeout in file '%s'",
          path)};
  }
  writeTimeout_ = Duration::fromMilliseconds(std::stoll(value));

#if 0
  if (!settings.load("director", "on-client-abort", value)) {
    clientAbortAction_ = ClientAbortAction::Close;
    logError("director: Could not load settings value "
             "director.on-client-abort  in file '{}'. Defaulting to '{}'.",
             path, tos(clientAbortAction_));
    ++changed;
  } else {
    Try<ClientAbortAction> t = parseClientAbortAction(value);
    if (t) {
      clientAbortAction_ = t.get();
    } else {
      clientAbortAction_ = ClientAbortAction::Close;
      logWarning(
          "director: Could not load settings value director.on-client-abort  "
          "in file '{}'. %s Defaulting to '{}'.",
          path, t.errorMessage(), tos(clientAbortAction_));
      ++changed;
    }
  }
#endif

  if (!settings.load("director", "max-retry-count", value)) {
    throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "director.max-retry-count in file '{}'",
          path)};
  }
  maxRetryCount_ = std::stoll(value);

  if (!settings.load("director", "sticky-offline-mode", value)) {
    throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "director.sticky-offline-mode in file '{}'",
          path)};
  }
  stickyOfflineMode_ = value == "true";

  if (!settings.load("director", "allow-x-sendfile", value)) {
    logError("director: Could not load settings value director.x-sendfile "
             "in file '{}'",
             path);
    allowXSendfile_ = false;
    ++changed;
  } else {
    allowXSendfile_ = value == "true";
  }

  if (!settings.load("director", "enqueue-on-unavailable", value)) {
    logError("director: Could not load settings value "
             "director.enqueue-on-unavailable in file '{}'",
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
        throw std::runtime_error{fmt::format(
            "director: Could not load settings value "
            "director.health-check-success-threshold in file '{}'",
            path)};
      }
    }
  }

  if (!settings.load("director", "health-check-host-header",
                     healthCheckHostHeader_)) {
    throw std::runtime_error{fmt::format(
        "director: Could not load settings value "
        "director.health-check-host-header in file '{}'",
        path)};
  }

  if (!settings.load("director", "health-check-request-path",
                     healthCheckRequestPath_)) {
    throw std::runtime_error{fmt::format(
        "director: Could not load settings value "
        "director.health-check-request-path in file '{}'",
        path)};
  }

  if (!settings.load("director", "health-check-fcgi-script-filename",
                     healthCheckFcgiScriptFilename_)) {
    healthCheckFcgiScriptFilename_ = "";
  }

  if (!settings.load("director", "scheduler", value)) {
    logWarning("director: Could not load configuration value for "
               "director.scheduler. Using default scheduler {}.",
               scheduler()->name());
    changed++;
  } else if (!setScheduler(value)) {
    ; // XXX ("Setting scheduler failed. Unknown scheduler {}", value)
  }

#if defined(ENABLE_DIRECTOR_CACHE)
  if (settings.contains("cache", "enabled")) {
    if (!settings.load("cache", "enabled", value)) {
      throw std::runtime_error{fmt::format(
          "director: Could not load settings value cache.enabled in file '{}'",
          path)};
    }
    objectCache().setEnabled(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "deliver-active")) {
    if (!settings.load("cache", "deliver-active", value)) {
      throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "cache.deliver-active in file '{}'",
          path)};
    }
    objectCache().setDeliverActive(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "deliver-shadow")) {
    if (!settings.load("cache", "deliver-shadow", value)) {
      throw std::runtime_error{fmt::format(
          "director: Could not load settings value "
          "cache.deliver-shadow in file '{}'",
          path)};
    }
    objectCache().setDeliverShadow(value == "true");
  } else {
    ++changed;
  }

  if (settings.contains("cache", "default-ttl")) {
    if (!settings.load("cache", "default-ttl", value)) {
      throw std::runtime_error{fmt::format(
          "director: Could not load settings value cache.default-ttl "
          "in file '{}'",
          path)};
    }
    objectCache().setDefaultTTL(Duration::fromMilliseconds(stoi(value)));
  } else {
    ++changed;
  }

  if (settings.contains("cache", "default-shadow-ttl")) {
    if (!settings.load("cache", "default-shadow-ttl", value)) {
      throw std::runtime_error{fmt::format(
          "director: Could not load settings value cache.default-ttl "
          "in file '{}'",
          path)};
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
      throw std::runtime_error{fmt::format(
          "director: Invalid configuration section '{}' in file '{}'.",
          key, path)};
    }
  }

  setMutable(true);

  if (changed) {
    logNotice("director: Rewriting configuration, as {} attribute(s) "
              "changed while loading.",
              changed);
    saveConfiguration();
  }
}

void Cluster::loadBackend(const IniFile& settings, const std::string& key) {
  std::string name = key.substr(strlen("backend="));

  TRACE("Cluster {}: loading backend: {}", name_, name);

  // capacity
  std::string capacityStr;
  if (!settings.load(key, "capacity", capacityStr)) {
      throw std::runtime_error{fmt::format(
        "director: Error loading configuration file '{}'. Item "
        "'capacity' not found in section '{}'.",
        storagePath_, key)};
  }
  size_t capacity = std::stoll(capacityStr);

  // protocol
  std::string protocol;
  if (!settings.load(key, "protocol", protocol)) {
    throw std::runtime_error{fmt::format(
        "director: Error loading configuration file '{}'. Item "
        "'protocol' not found in section '{}'.",
        storagePath_, key)};
  }

  // enabled
  std::string enabledStr;
  if (!settings.load(key, "enabled", enabledStr)) {
    throw std::runtime_error{fmt::format(
          "director: Error loading configuration file '{}'. Item "
          "'enabled' not found in section '{}'.",
          storagePath_, key)};
  }
  bool enabled = enabledStr == "true";

  // health-check-interval
  std::string hcIntervalStr;
  if (!settings.load(key, "health-check-interval", hcIntervalStr)) {
    throw std::runtime_error{fmt::format(
        "director: Error loading configuration file '{}'. Item "
        "'health-check-interval' not found in section '{}'.",
        storagePath_, key)};
  }
  Duration hcInterval = Duration::fromMilliseconds(std::stoll(hcIntervalStr));

  // host
  std::string host;
  if (!settings.load(key, "host", host)) {
    throw std::runtime_error{fmt::format(
        "director: Error loading configuration file '{}'. Item "
        "'host' not found in section '{}'.",
        storagePath_, key)};
  }

  // port
  std::string portStr;
  if (!settings.load(key, "port", portStr)) {
    throw std::runtime_error{fmt::format(
        "director: Error loading configuration file '{}'. Item "
        "'port' not found in section '{}'.",
        storagePath_, key)};
  }

  int port = std::atoi(portStr.c_str());
  if (port <= 0) {
    throw std::runtime_error{fmt::format(
        "director: Error loading configuration file '{}'. Invalid "
        "port number '{}' for backend '{}'",
        storagePath_, portStr, name)};
  }
  InetAddress addr(host, port);

  bool terminateProtection = false; // TODO: make configurable

  // spawn backend (by protocol)
  addMember(name, addr, capacity, enabled, terminateProtection,
            protocol, hcInterval);
}

void Cluster::loadBucket(const IniFile& settings, const std::string& key) {
  std::string name = key.substr(strlen("bucket="));

  std::string rateStr;
  if (!settings.load(key, "rate", rateStr)) {
    throw std::runtime_error{fmt::format(
        "director: Error loading configuration file '{}'. Item "
        "'rate' not found in section '{}'.",
        storagePath_, key)};
  }

  std::string ceilStr;
  if (!settings.load(key, "ceil", ceilStr)) {
    throw std::runtime_error{fmt::format(
        "director: Error loading configuration file '{}'. Item "
        "'ceil' not found in section '{}'.",
        storagePath_, key)};
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
    throw std::runtime_error{fmt::format(
        "Could not create director's bucket. {}",
        str[(size_t)ec])};
  }
}

bool Cluster::setScheduler(const std::string& value) {
  if (value == "rr") {
    setScheduler(std::make_unique<Scheduler::RoundRobin>(&members_));
    return true;
  } else if (value == "chance") {
    setScheduler(std::make_unique<Scheduler::Chance>(&members_));
    return true;
  } else {
    return false;
  }
}

void Cluster::setScheduler(std::unique_ptr<Scheduler> scheduler) {
  scheduler_ = std::move(scheduler);
}

void Cluster::addMember(const InetAddress& addr) {
  addMember(fmt::format("{}", addr),
            addr,
            0,        // capacity (0 = no limit)
            true,     // enabled
            false,    // terminateProtection
            "http",   // protocol
            healthCheckInterval_);
}

void Cluster::addMember(const InetAddress& addr, size_t capacity) {
  addMember(fmt::format("{}", addr),
            addr,
            capacity,
            true,     // enabled
            false,    // terminateProtection
            "http",   // protocol
            healthCheckInterval_);
}

void Cluster::addMember(const std::string& name,
                        const InetAddress& addr,
                        size_t capacity,
                        bool enabled,
                        bool terminateProtection,
                        const std::string& protocol,
                        Duration healthCheckInterval) {
  Executor* const executor = executor_; // TODO: get as function arg for passing: daemon().selectClientScheduler()

  TRACE("addMember: {} {}", name, addr);

  std::unique_ptr<Backend> backend = std::make_unique<Backend>(
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

  members_.push_back(std::move(backend));
}

Backend* Cluster::findMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const std::unique_ptr<Backend>& m) -> bool { return m->name() == name; });
  if (i != members_.end()) {
    return i->get();
  } else {
    return nullptr;
  }
}

void Cluster::removeMember(const std::string& name) {
  auto i = std::find_if(members_.begin(), members_.end(),
      [&](const std::unique_ptr<Backend>& m) -> bool { return m->name() == name; });
  if (i != members_.end()) {
    members_.erase(i);
  }
}

void Cluster::setHealthCheckHostHeader(const std::string& value) {
  healthCheckHostHeader_ = value;

  for (std::unique_ptr<Backend>& member: members_) {
    member->healthMonitor()->setHostHeader(value);
  }
}

void Cluster::setHealthCheckRequestPath(const std::string& value) {
  healthCheckRequestPath_ = value;

  for (std::unique_ptr<Backend>& member: members_) {
    member->healthMonitor()->setRequestPath(value);
  }
}

void Cluster::setHealthCheckInterval(Duration value) {
  healthCheckInterval_ = value;

  for (std::unique_ptr<Backend>& member: members_) {
    member->healthMonitor()->setInterval(value);
  }
}

void Cluster::setHealthCheckSuccessThreshold(unsigned value) {
  healthCheckSuccessThreshold_ = value;

  for (std::unique_ptr<Backend>& member: members_) {
    member->healthMonitor()->setSuccessThreshold(value);
  }
}

void Cluster::setHealthCheckSuccessCodes(const std::vector<HttpStatus>& value) {
  healthCheckSuccessCodes_ = value;

  for (std::unique_ptr<Backend>& member: members_) {
    member->healthMonitor()->setSuccessCodes(value);
  }
}

TokenShaperError Cluster::createBucket(const std::string& name, float rate,
                                       float ceil) {
  return shaper_.createNode(name, rate, ceil);
}

Cluster::Bucket* Cluster::findBucket(const std::string& name) const {
  return shaper_.findNode(name);
}

bool Cluster::eachBucket(std::function<bool(Bucket*)> body) {
  for (auto& node : *shaper_.rootNode())
    if (!body(node.get()))
      return false;

  return true;
}

void Cluster::schedule(Context* cx) {
  schedule(cx, rootBucket());
}

void Cluster::schedule(Context* cx, Bucket* bucket) {
  cx->bucket = bucket;

  if (!enabled_) {
    serviceUnavailable(cx);
    return;
  }

  if (cx->bucket->get(1)) {
    cx->tokens = 1;
    SchedulerStatus status = scheduler()->schedule(cx);
    if (status == SchedulerStatus::Success)
      return;

    cx->bucket->put(1);
    cx->tokens = 0;

    if (status == SchedulerStatus::Unavailable &&
        !enqueueOnUnavailable_) {
      serviceUnavailable(cx);
    } else {
      enqueue(cx);
    }
  } else if (cx->bucket->ceil() > 0 || enqueueOnUnavailable_) {
    // there are tokens available (for rent) and we prefer to wait if unavailable
    enqueue(cx);
  } else {
    serviceUnavailable(cx);
  }
}

void Cluster::reschedule(Context* cx) {
  TRACE("reschedule");

  if (verifyTryCount(cx)) {
    SchedulerStatus status = scheduler()->schedule(cx);

    if (status != SchedulerStatus::Success) {
      enqueue(cx);
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
bool Cluster::verifyTryCount(Context* cx) {
  if (cx->tryCount <= maxRetryCount())
    return true;

  TRACE("proxy.cluster %s: request failed %d times.", name().c_str(), cx->tryCount);
  serviceUnavailable(cx);
  return false;
}

void Cluster::serviceUnavailable(Context* cx, HttpStatus status) {
  cx->onMessageBegin(HttpVersion::VERSION_1_1,
                     status,
                     BufferRef(to_string(status)));

  // TODO: put into a more generic place where it affects all responses.
  if (cx->bucket) {
    cx->onMessageHeader(BufferRef("Cluster-Bucket"),
                        BufferRef(cx->bucket->name()));
  }

  if (retryAfter() != Duration::Zero) {
    cx->onMessageHeader(
        BufferRef("Retry-After"),
        BufferRef(to_string(retryAfter().seconds())));
  }

  cx->onMessageHeaderEnd();
  cx->onMessageEnd();

  ++dropped_;
}

/**
 * Attempts to enqueue the request, respecting limits.
 *
 * Attempts to enqueue the request on the associated bucket.
 * If enqueuing fails, it instead finishes the request with a 503 (Service
 * Unavailable).
 */
void Cluster::enqueue(Context* cx) {
  if (cx->bucket->queued().current() < queueLimit()) {
    cx->backend = nullptr;
    cx->bucket->enqueue(cx);
    ++queued_;

    DEBUG("HTTP cluster {} [{}] overloaded. Enqueueing request ({}).",
          name(),
          cx->bucket->name(),
          cx->bucket->queued().current());
  } else {
    DEBUG("director: '{}' queue limit {} reached.", name(), queueLimit());
    serviceUnavailable(cx);
  }
}

/**
 * Pops an enqueued request from the front of the queue and passes it to the
 * backend for serving.
 *
 * @param backend the backend to pass the dequeued request to.
 */
void Cluster::dequeueTo(Backend* backend) {
  if (auto cx = dequeue()) {
    cx->post([this, backend, cx]() {
      cx->tokens = 1;
      DEBUG("Dequeueing request to backend {} @ {} ({})",
          backend->name(), name(), queued_.current());
      SchedulerStatus rc = backend->tryProcess(cx);
      if (rc != SchedulerStatus::Success) {
        cx->tokens = 0;
        logError("Dequeueing request to backend {} @ {} failed. {}",
                 backend->name(), name(), rc);
        reschedule(cx);
      } else {
        // FIXME: really here????
        verifyTryCount(cx);
      }
    });
  } else {
    TRACE("dequeueTo: queue empty.");
  }
}

Context* Cluster::dequeue() {
  if (auto cx = shaper()->dequeue()) {
    --queued_;
    return cx;
  }

  return nullptr;
}

void Cluster::onTimeout(Context* cx) {
  --queued_;

  cx->post([this, cx]() {
    Duration diff = MonotonicClock::now() - cx->ctime;
    logInfo("Queued request timed out ({}). {} {}",
            diff,
            cx->request.method(),
            cx->request.path());

    serviceUnavailable(cx, HttpStatus::GatewayTimeout);
  });
}

// {{{ EventListener overrides
void Cluster::onEnabledChanged(Backend* backend) {
  DEBUG("onBackendEnabledChanged: {} {}",
        backend->name(), backend->isEnabled() ? "enabled" : "disabled");
  TRACE("onBackendEnabledChanged: {} {}",
        backend->name(), backend->isEnabled() ? "enabled" : "disabled");

  if (backend->isEnabled()) {
    shaper()->resize(shaper()->size() + backend->capacity());
  } else {
    shaper()->resize(shaper()->size() - backend->capacity());
  }
}

void Cluster::onCapacityChanged(Backend* member, size_t old) {
  if (member->isEnabled()) {
    TRACE("onCapacityChanged: member {} capacity {}", member->name(), member->capacity());
    shaper()->resize(shaper()->size() - old + member->capacity());
  }
}

void Cluster::onHealthChanged(Backend* backend, HealthMonitor::State oldState) {
  logInfo("HTTP cluster {}: backend '{}' ({}:{}) is now {}.",
          name(), backend->name(),
          backend->inetAddress().ip(),
          backend->inetAddress().port(),
          backend->healthMonitor()->state());

  if (!backend->isEnabled())
    return;

  if (backend->healthMonitor()->isOnline()) {
    // backend is online and enabled

    TRACE("onHealthChanged: adding capacity to shaper ({} + {})",
           shaper()->size(), backend->capacity());

    shaper()->resize(shaper()->size() + backend->capacity());

    if (!stickyOfflineMode()) {
      // try delivering a queued request
      dequeueTo(backend);
    } else {
      // disable backend due to sticky-offline mode
      logNotice(
          "HTTP cluster {}: backend '{}' disabled due to sticky offline mode.",
          name(), backend->name());
      backend->setEnabled(false);
    }
  } else if (oldState == HealthMonitor::State::Online) {
    // backend is offline and enabled
    TRACE("onHealthChanged: removing capacity from shaper ({} - {})",
          shaper()->size(), backend->capacity());
    shaper()->resize(shaper()->size() - backend->capacity());
  }
}

void Cluster::onProcessingSucceed(Backend* member) {
  dequeueTo(member);
}

void Cluster::onProcessingFailed(Context* request) {
  assert(request->bucket != nullptr);
  assert(request->tokens != 0);

  request->bucket->put(1);
  request->tokens = 0;

  reschedule(request);
}
// }}}

void Cluster::serialize(JsonWriter& json) const {
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

} // namespace xzero::http::cluster

namespace xzero {
  template<>
  JsonWriter& JsonWriter::value(const http::cluster::Cluster& cluster) {
    cluster.serialize(*this);
    return *this;
  }
} // namespace xzero
