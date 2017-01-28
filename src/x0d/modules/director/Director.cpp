// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "Director.h"
#include "Backend.h"
#include "HttpBackend.h"
#include "RequestNotes.h"
#include "FastCgiBackend.h"

#include <x0d/sysconfig.h>

#if defined(ENABLE_DIRECTOR_CACHE)
#include "ObjectCache.h"
#endif

#include <base/io/BufferSource.h>
#include <base/DebugLogger.h>
#include <base/Tokenizer.h>
#include <base/IniFile.h>
#include <base/Url.h>
#include <fstream>

#include <base/AnsiColor.h>  // dumpNode()

using namespace base;
using namespace xzero;

template <typename... Args>
static inline void logRequest(HttpRequest* r, int level, const char* fmt,
                              const Args&... args) {
  LogMessage msg(level, fmt, args...);
  msg.addTag("director");
  r->log(std::move(msg));
}

#if !defined(NDEBUG)
//#	define TRACE(obj, level, msg...) (obj)->request->log(Severity::trace ##
//level, "director: " msg)
//#	define WTRACE(level, msg...) worker_->log(Severity::trace ## level,
//"director: " msg)
#define TRACE(obj, level, msg...) ::logRequest((obj)->request, level, msg)
#define WTRACE(level, msg...)                  \
  do {                                         \
    LogMessage m(Severity::trace##level, msg); \
    m.addTag("director");                      \
    worker_->log(std::move(m));                \
  } while (0)
#else
#define TRACE(args...) \
  do {                 \
  } while (0)
#define WTRACE(args...) \
  do {                  \
  } while (0)
#endif

using namespace base;

struct BackendData : public CustomData {
  BackendRole role;

  BackendData() : role() {}

  virtual ~BackendData() {}
};

static inline const std::string& role2str(BackendRole role) {
  static std::string map[] = {"active", "standby", "backup", "terminate", };
  return map[static_cast<size_t>(role)];
}

/**
 * Initializes a director object (load balancer instance).
 *
 * \param worker the worker associated with this director's local jobs (e.g.
 *backend health checks).
 * \param name the unique human readable name of this director (load balancer)
 *instance.
 */
Director::Director(HttpWorker* worker, const std::string& name)
    : BackendManager(worker, name),
      mutable_(false),
      healthCheckHostHeader_("backend-healthcheck"),
      healthCheckRequestPath_("/"),
      healthCheckFcgiScriptFilename_(),
      enabled_(true),
      stickyOfflineMode_(false),
      allowXSendfile_(false),  // disabled by default for security reasons
      enqueueOnUnavailable_(false),
      backends_(),
      queueLimit_(128),
      queueTimeout_(60_seconds),
      retryAfter_(10_seconds),
      maxRetryCount_(6),
      storagePath_(),
      shaper_(worker->loop(), 0),
      queued_(),
      dropped_(0),
#if defined(ENABLE_DIRECTOR_CACHE)
      objectCache_(nullptr),
#endif
      stopHandle_() {
  backends_.resize(3);

  stopHandle_ = worker->registerStopHandler(std::bind(&Director::onStop, this));

  shaper_.setTimeoutHandler(
      std::bind(&Director::onTimeout, this, std::placeholders::_1));

#if defined(ENABLE_DIRECTOR_CACHE)
  objectCache_ = new ObjectCache(this);
#endif
}

Director::~Director() {
  worker()->unregisterStopHandler(stopHandle_);

  for (auto& backendRole : backends_) {
    backendRole.each([&](Backend* b) { delete b; });
  }

#if defined(ENABLE_DIRECTOR_CACHE)
  delete objectCache_;
#endif
}

/** Callback, that updates shaper capacity based on enabled/health state.
 *
 * This callback gets invoked when a backend's enabled state toggled between
 * the values \c true and \c false.
 * It will then try to either increase the shaping capacity or reduce it.
 *
 * \see onBackendStateChanged
 */
void Director::onBackendEnabledChanged(const Backend* backend) {
  WTRACE(1, "onBackendEnabledChanged: health=$0, enabled=$1",
         backend->healthMonitor()->state(),
         backend->isEnabled());

  if (backendRole(backend) != BackendRole::Active) return;

  if (backend->healthMonitor()->isOnline()) {
    if (backend->isEnabled()) {
      WTRACE(1,
             "onBackendEnabledChanged: adding capacity to shaper (%zi + %zi)",
             shaper()->size(), backend->capacity());
      shaper()->resize(shaper()->size() + backend->capacity());
    } else {
      WTRACE(
          1,
          "onBackendEnabledChanged: removing capacity from shaper (%zi - %zi)",
          shaper()->size(), backend->capacity());
      shaper()->resize(shaper()->size() - backend->capacity());
    }
  }
}

void Director::onBackendStateChanged(Backend* backend,
                                     HealthMonitor* healthMonitor,
                                     HealthState oldState) {
  WTRACE(1, "onBackendStateChanged: health=$0 -> $1, enabled=$2",
         oldState,
         backend->healthMonitor()->state(),
         backend->isEnabled());

  worker_->log(Severity::info, "Director '%s': backend '%s' is now %s.",
               name().c_str(), backend->name_.c_str(),
               healthMonitor->state_str().c_str());

  if (healthMonitor->isOnline()) {
    if (!backend->isEnabled()) return;

    // backend is online and enabled

    WTRACE(1, "onBackendStateChanged: adding capacity to shaper ($0 + $1)",
           shaper()->size(), backend->capacity());
    shaper()->resize(shaper()->size() + backend->capacity());

    if (!stickyOfflineMode()) {
      // try delivering a queued request
      dequeueTo(backend);
    } else {
      // disable backend due to sticky-offline mode
      worker_->log(
          Severity::notice,
          "Director '%s': backend '%s' disabled due to sticky offline mode.",
          name().c_str(), backend->name().c_str());
      backend->setEnabled(false);
    }
  } else if (backend->isEnabled() && oldState == HealthState::Online) {
    // backend is offline and enabled
    shaper()->resize(shaper()->size() - backend->capacity());
    WTRACE(1,
           "onBackendStateChanged: removing capacity from shaper (%zi - %zi)",
           shaper()->size(), backend->capacity());
  }
}

void Director::reject(RequestNotes* rn, HttpStatus /*status*/) {
  // we ignore the reject-status here, as we attempt to reschedule the request.
  // if rescheduling fails, a more appropriate response status will be chosen
  // instead.

  reschedule(rn);
}

/**
 * Notifies the director, that the given backend has just completed processing a
 *request.
 *
 * @param backend the backend that just completed a request, and thus, is able
 *to potentially handle one more.
 * @param r the request that just got completed.
 *
 * This method is to be invoked by backends, that
 * just completed serving a request, and thus, invoked
 * finish() on it, so it could potentially process
 * the next one, if, and only if, we have
 * already queued pending requests.
 *
 * Otherwise this call will do nothing.
 *
 * @see Backend::release()
 */
void Director::release(RequestNotes* rn) {
  auto backend = rn->backend;

  --load_;

  // explicitely clear reuqest notes here, so we also free up its acquired
  // shaper tokens
  if (rn->bucket && rn->tokens) {
    rn->bucket->put(rn->tokens);
    rn->tokens = 0;
  }

  if (backendRole(backend) != BackendRole::Terminate) {
    dequeueTo(backend);
  } else if (backend->load().current() == 0) {
    backend->healthMonitor()->stop();
    delete backend;
    save();
  }
}

/**
 * Callback, invoked when the owning worker thread is to be stopped.
 *
 * We're unregistering any possible I/O watchers and timers, as used
 * by proxying connections and health checks.
 */
void Director::onStop() {
  WTRACE(1, "onStop()");

  for (auto& br : backends_) {
    br.each([&](Backend* backend) {
      backend->disable();
      backend->healthMonitor()->stop();
    });
  }
}

size_t Director::capacity() const {
  size_t result = 0;

  for (const auto& br : backends_) result += br.capacity();

  return result;
}

TokenShaperError Director::createBucket(const std::string& name, float rate,
                                            float ceil) {
  return shaper_.createNode(name, rate, ceil);
}

RequestShaper::Node* Director::findBucket(const std::string& name) const {
  return shaper_.findNode(name);
}

bool Director::eachBucket(std::function<bool(RequestShaper::Node*)> body) {
  for (auto& node : *shaper_.rootNode())
    if (!body(node)) return false;

  return true;
}

Backend* Director::createBackend(const std::string& name, const Url& url) {
  SocketSpec spec = SocketSpec::fromInet(IPAddress(url.hostname()), url.port());
  int capacity = 1;
  BackendRole role = BackendRole::Active;

  return createBackend(name, url.protocol(), spec, capacity, role);
}

Backend* Director::createBackend(const std::string& name,
                                 const std::string& protocol,
                                 const SocketSpec& socketSpec,
                                 size_t capacity, BackendRole role) {
  if (findBackend(name)) return nullptr;

  Backend* backend;

  if (protocol == "fastcgi") {
    backend = new FastCgiBackend(this, name, socketSpec, capacity, true);
  } else if (protocol == "http") {
    backend = new HttpBackend(this, name, socketSpec, capacity, true);
  } else {
    return nullptr;
  }

  backend->disable();  // ensure backend is disabled upon creation

  BackendData* bd = backend->setCustomData<BackendData>(this);
  bd->role = role;

  link(backend, role);

  backend->setEnabledCallback([this](const Backend* backend) {
    onBackendEnabledChanged(backend);
  });

  backend->healthMonitor()->setStateChangeCallback([this, backend](
      HealthMonitor*, HealthState oldState) {
    onBackendStateChanged(backend, backend->healthMonitor(), oldState);
  });

  backend->setJsonWriteCallback([role](
      const Backend*, JsonWriter& json) { json.name("role")(role2str(role)); });

  // wake up the worker's event loop here, so he knows in time about the health
  // check timer we just installed.
  // TODO we should not need this...
  worker()->wakeup();

  return backend;
}

void Director::terminateBackend(Backend* backend) {
  setBackendRole(backend, BackendRole::Terminate);
}

void Director::link(Backend* backend, BackendRole role) {
  BackendData* data = backend->customData<BackendData>(this);
  data->role = role;

  backends_[static_cast<size_t>(role)].push_back(backend);
}

Backend* Director::unlink(Backend* backend) {
  auto& br = backends_[static_cast<size_t>(backendRole(backend))];
  br.remove(backend);
  return backend;
}

BackendRole Director::backendRole(const Backend* backend) const {
  return backend->customData<BackendData>(this)->role;
}

bool Director::findBackend(const std::string& name,
                           const std::function<void(Backend*)>& cb) {
  for (auto& br : backends_) {
    if (auto b = br.find(name)) {
      cb(b);
      return true;
    }
  }

  return false;
}

Backend* Director::findBackend(const std::string& name) {
  for (auto& br : backends_)
    if (auto b = br.find(name)) return b;

  return nullptr;
}

void Director::setBackendRole(Backend* backend, BackendRole role) {
  BackendRole currentRole = backendRole(backend);
  WTRACE(1, "setBackendRole($0) (from $1)", role, currentRole);

  if (role != currentRole) {
    if (role == BackendRole::Active)
      shaper()->resize(shaper()->size() + backend->capacity());
    else
      shaper()->resize(shaper()->size() - backend->capacity());

    if (role == BackendRole::Terminate) {
      unlink(backend);

      if (backend->load().current()) {
        link(backend, role);
      } else {
        delete backend;
        save();
      }
    } else {
      unlink(backend);
      link(backend, role);
    }
  }
}

void Director::writeJSON(JsonWriter& json) const {
  json.beginObject()
      .name("mutable")(isMutable())
      .name("enabled")(isEnabled())
      .name("queue-limit")(queueLimit_)
      .name("queue-timeout")(queueTimeout_.totalSeconds())
      .name("on-client-abort")(tos(clientAbortAction()))
      .name("retry-after")(retryAfter_.totalSeconds())
      .name("max-retry-count")(maxRetryCount_)
      .name("sticky-offline-mode")(stickyOfflineMode_)
      .name("allow-x-sendfile")(allowXSendfile_)
      .name("enqueue-on-unavailable")(enqueueOnUnavailable_)
      .name("connect-timeout")(connectTimeout_.totalSeconds())
      .name("read-timeout")(readTimeout_.totalSeconds())
      .name("write-timeout")(writeTimeout_.totalSeconds())
      .name("health-check-host-header")(healthCheckHostHeader_)
      .name("health-check-request-path")(healthCheckRequestPath_)
      .name("health-check-fcgi-script-name")(healthCheckFcgiScriptFilename_)
      .name("scheduler")(scheduler())
      .beginObject("stats")
      .name("load")(load_)
      .name("queued")(queued_)
      .name("dropped")(dropped_)
      .endObject()
#if defined(ENABLE_DIRECTOR_CACHE)
      .name("cache")(*objectCache_)
#endif
      .name("shaper")(shaper_)
      .beginArray("members");

  for (auto& br : backends_) {
    br.each([&](const Backend* backend) { json.value(*backend); });
  }

  json.endArray();
  json.endObject();
}

namespace base {
JsonWriter& operator<<(JsonWriter& json, const Director& director) {
  director.writeJSON(json);
  return json;
}
}

/**
 * Loads director configuration from given file.
 *
 * \param path The path to the file holding the configuration.
 *
 * \retval true Success.
 * \retval false Failed, detailed message in errno.
 */
bool Director::load(const std::string& path) {
  // treat director as loaded if db file behind given path does not exist
  struct stat st;
  if (stat(path.c_str(), &st) < 0 && errno == ENOENT) {
    storagePath_ = path;
    setMutable(true);
    return save();
  }

  storagePath_ = path;

  size_t changed = 0;
  IniFile settings;
  if (!settings.loadFile(path)) {
    worker()->log(
        Severity::error,
        "director: Could not load director settings from file '%s'. %s",
        path.c_str(), strerror(errno));
    return false;
  }

  std::string value;

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
    save();
  }

  return true;
}

bool Director::loadBucket(const IniFile& settings, const std::string& key) {
  std::string name = key.substr(strlen("bucket="));

  std::string rateStr;
  if (!settings.load(key, "rate", rateStr)) {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'rate' not found in section '%s'.",
                  storagePath_.c_str(), key.c_str());
    return false;
  }

  std::string ceilStr;
  if (!settings.load(key, "ceil", ceilStr)) {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'ceil' not found in section '%s'.",
                  storagePath_.c_str(), key.c_str());
    return false;
  }

  char* nptr = nullptr;
  float rate = strtof(rateStr.c_str(), &nptr);
  float ceil = strtof(ceilStr.c_str(), &nptr);

  TokenShaperError ec = createBucket(name, rate, ceil);
  if (ec != TokenShaperError::Success) {
    static const char* str[] = {"Success.",             "Rate limit overflow.",
                                "Ceil limit overflow.", "Name conflict.",
                                "Invalid child node.", };
    worker()->log(Severity::error, "Could not create director's bucket. %s",
                  str[(size_t)ec]);
    return false;
  }

  return true;
}

bool Director::loadBackend(const IniFile& settings, const std::string& key) {
  std::string name = key.substr(strlen("backend="));

  // role
  std::string roleStr;
  if (!settings.load(key, "role", roleStr)) {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'role' not found in section '%s'.",
                  storagePath_.c_str(), key.c_str());
    return false;
  }

  BackendRole role = BackendRole::Terminate;  // aka. Undefined
  if (roleStr == "active")
    role = BackendRole::Active;
  else if (roleStr == "backup")
    role = BackendRole::Backup;
  else {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'role' for backend '%s' contains invalid data '%s'.",
                  storagePath_.c_str(), key.c_str(), roleStr.c_str());
    return false;
  }

  // capacity
  std::string capacityStr;
  if (!settings.load(key, "capacity", capacityStr)) {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'capacity' not found in section '%s'.",
                  storagePath_.c_str(), key.c_str());
    return false;
  }
  size_t capacity = std::atoll(capacityStr.c_str());

  // protocol
  std::string protocol;
  if (!settings.load(key, "protocol", protocol)) {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'protocol' not found in section '%s'.",
                  storagePath_.c_str(), key.c_str());
    return false;
  }

  // enabled
  std::string enabledStr;
  if (!settings.load(key, "enabled", enabledStr)) {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'enabled' not found in section '%s'.",
                  storagePath_.c_str(), key.c_str());
    return false;
  }
  bool enabled = enabledStr == "true";

  // health-check-interval
  std::string hcIntervalStr;
  if (!settings.load(key, "health-check-interval", hcIntervalStr)) {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'health-check-interval' not found in section '%s'.",
                  storagePath_.c_str(), key.c_str());
    return false;
  }
  Duration hcInterval =
      Duration::fromSeconds(std::atoll(hcIntervalStr.c_str()));

  // health-check-mode
  std::string hcModeStr;
  if (!settings.load(key, "health-check-mode", hcModeStr)) {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'health-check-mode' not found in section '%s'.",
                  storagePath_.c_str(), hcModeStr.c_str(), key.c_str());
    return false;
  }

  HealthMonitor::Mode hcMode;
  if (hcModeStr == "paranoid")
    hcMode = HealthMonitor::Mode::Paranoid;
  else if (hcModeStr == "opportunistic")
    hcMode = HealthMonitor::Mode::Opportunistic;
  else if (hcModeStr == "lazy")
    hcMode = HealthMonitor::Mode::Lazy;
  else {
    worker()->log(Severity::error,
                  "director: Error loading configuration file '%s'. Item "
                  "'health-check-mode' invalid ('%s') in section '%s'.",
                  storagePath_.c_str(), hcModeStr.c_str(), key.c_str());
    return false;
  }

  SocketSpec socketSpec;
  std::string path;
  if (settings.load(key, "path", path)) {
    socketSpec = SocketSpec::fromLocal(path);
  } else {
    // host
    std::string host;
    if (!settings.load(key, "host", host)) {
      worker()->log(Severity::error,
                    "director: Error loading configuration file '%s'. Item "
                    "'host' not found in section '%s'.",
                    storagePath_.c_str(), key.c_str());
      return false;
    }

    // port
    std::string portStr;
    if (!settings.load(key, "port", portStr)) {
      worker()->log(Severity::error,
                    "director: Error loading configuration file '%s'. Item "
                    "'port' not found in section '%s'.",
                    storagePath_.c_str(), key.c_str());
      return false;
    }

    int port = std::atoi(portStr.c_str());
    if (port <= 0) {
      worker()->log(Severity::error,
                    "director: Error loading configuration file '%s'. Invalid "
                    "port number '%s' for backend '%s'",
                    storagePath_.c_str(), portStr.c_str(), name.c_str());
      return false;
    }

    socketSpec = SocketSpec::fromInet(IPAddress(host), port);
  }

  // spawn backend (by protocol)
  Backend* backend = createBackend(name, protocol, socketSpec, capacity, role);
  if (!backend) {
    worker()->log(Severity::error,
                  "director: Invalid protocol '%s' for backend '%s' in "
                  "configuration file '%s'.",
                  protocol.c_str(), name.c_str(), storagePath_.c_str());
    return false;
  }

  backend->setEnabled(enabled);
  backend->healthMonitor()->setMode(hcMode);
  backend->healthMonitor()->setInterval(hcInterval);

  return true;
}

/**
 * stores director configuration in a plaintext file.
 *
 * \todo this must happen asynchronousely, never ever block within the callers
 *thread (or block in a dedicated thread).
 */
bool Director::save() {
  static const std::string header(
      "name,role,capacity,protocol,enabled,transport,host,port");
  std::string path = storagePath_;
  std::ofstream out(path, std::ios_base::out | std::ios_base::trunc);

  out << "# vim:syntax=dosini\n"
      << "# !!! DO NOT EDIT !!! THIS FILE IS GENERATED AUTOMATICALLY !!!\n\n"
      << "[director]\n"
      << "enabled=" << (enabled_ ? "true" : "false") << "\n"
      << "queue-limit=" << queueLimit_ << "\n"
      << "queue-timeout=" << queueTimeout_.totalSeconds() << "\n"
      << "on-client-abort=" << tos(clientAbortAction_) << "\n"
      << "retry-after=" << retryAfter_.totalSeconds() << "\n"
      << "max-retry-count=" << maxRetryCount_ << "\n"
      << "sticky-offline-mode=" << (stickyOfflineMode_ ? "true" : "false")
      << "\n"
      << "allow-x-sendfile=" << (allowXSendfile_ ? "true" : "false") << "\n"
      << "enqueue-on-unavailable=" << (enqueueOnUnavailable_ ? "true" : "false")
      << "\n"
      << "connect-timeout=" << connectTimeout_.totalSeconds() << "\n"
      << "read-timeout=" << readTimeout_.totalSeconds() << "\n"
      << "write-timeout=" << writeTimeout_.totalSeconds() << "\n"
      << "health-check-host-header=" << healthCheckHostHeader_ << "\n"
      << "health-check-request-path=" << healthCheckRequestPath_ << "\n"
      << "health-check-fcgi-script-filename=" << healthCheckFcgiScriptFilename_
      << "\n"
      << "scheduler=" << scheduler() << "\n"
      << "\n";

#if defined(ENABLE_DIRECTOR_CACHE)
  out << "[cache]\n"
      << "enabled=" << (objectCache().enabled() ? "true" : "false") << "\n"
      << "deliver-active=" << (objectCache().deliverActive() ? "true" : "false")
      << "\n"
      << "deliver-shadow=" << (objectCache().deliverShadow() ? "true" : "false")
      << "\n"
      << "default-ttl=" << objectCache().defaultTTL().totalSeconds() << "\n"
      << "default-shadow-ttl="
      << objectCache().defaultShadowTTL().totalSeconds() << "\n"
      << "\n";
#endif

  for (auto& bucket : *shaper()->rootNode()) {
    out << "[bucket=" << bucket->name() << "]\n"
        << "rate=" << bucket->rateP() << "\n"
        << "ceil=" << bucket->ceilP() << "\n"
        << "\n";
  }

  for (auto& br : backends_) {
    br.each([&](Backend* b) {
      out << "[backend=" << b->name() << "]\n"
          << "role=" << role2str(backendRole(b)) << "\n"
          << "capacity=" << b->capacity() << "\n"
          << "enabled=" << (b->isEnabled() ? "true" : "false") << "\n"
          << "transport=" << (b->socketSpec().isLocal() ? "local" : "tcp")
          << "\n"
          << "protocol=" << b->protocol() << "\n"
          << "health-check-mode=" << b->healthMonitor()->mode_str() << "\n"
          << "health-check-interval="
          << b->healthMonitor()->interval().totalSeconds() << "\n";

      if (b->socketSpec().isInet()) {
        out << "host=" << b->socketSpec().ipaddr().str() << "\n";
        out << "port=" << b->socketSpec().port() << "\n";
      } else {
        out << "path=" << b->socketSpec().local() << "\n";
      }

      out << "\n";
    });
  }

  return true;
}

const std::string& Director::scheduler() const {
  // it is safe to just query the first backend-role, as we currently only
  // support one scheduler type for all
  return backends_[0].scheduler()->name();
}

bool Director::setScheduler(const std::string& name) {
  if (name == scheduler()) return true;

  if (name == "chance") {
    setScheduler<ChanceScheduler>();
    return true;
  }

  if (name == "rr") {
    setScheduler<RoundRobinScheduler>();
    return true;
  }

  return false;
}

/**
 * Schedules a new request to be directly processed by a given backend.
 *
 * This has not much to do with scheduling, especially where the target backend
 * has been already chosen. This target backend must be used or an error
 * has to be served, e.g. when this backend is offline, disabled, or
 *overloarded.
 *
 * This request will be attempted to be scheduled on this backend only once.
 */
void Director::schedule(RequestNotes* notes, Backend* backend) {
  auto r = notes->request;
  notes->backend = backend;

  r->responseHeaders.push_back("X-Director-Bucket", notes->bucket->name());

  switch (backend->tryProcess(notes)) {
    case SchedulerStatus::Unavailable:
    case SchedulerStatus::Overloaded:
      r->log(Severity::error,
             "director: Requested backend '%s' is %s, and is unable to process "
             "requests (attempt %d).",
             backend->name().c_str(),
             backend->healthMonitor()->state_str().c_str(), notes->tryCount);
      serviceUnavailable(notes);

      // TODO: consider backend-level queues as a feature here (post 0.7
      // release)
      break;
    case SchedulerStatus::Success:
      break;
  }
}

#if defined(ENABLE_DIRECTOR_CACHE)
/*!
 * Validates request against a possibly existing cached object and delivers it
 *or requests updating it.
 *
 * \retval true request has been processed and will receive a cached object.
 * \retval false the request must be processed by a backend server, possibly
 *updating the corresponding stale cache object.
 */
bool Director::processCacheObject(RequestNotes* notes) {
  auto r = notes->request;

  // are we caching at all?
  if (!objectCache_->enabled()) return false;

  if (unlikely(notes->cacheKey.empty())) notes->setCacheKey("%h#%r#%q");

  if (!notes->cacheTTL) notes->cacheTTL = objectCache_->defaultTTL();

  // is this a PURGE request?
  if (unlikely(equals(r->method, "PURGE"))) {
    if (objectCache_->purge(notes->cacheKey)) {
      r->status = HttpStatus::Ok;
      r->finish();
    } else {
      r->status = HttpStatus::NotFound;
      r->finish();
    }

    return true;
  }

  // should this request be ignored by cache?
  if (unlikely(notes->cacheIgnore)) return false;

  // is the request method allowed for caching?
  static const char* allowedMethods[] = {"GET", "HEAD"};
  bool methodFound = false;
  for (auto& method : allowedMethods) {
    if (equals(r->method, method)) {
      methodFound = true;
      break;
    }
  }

  if (!methodFound) return false;

  // finally actually deliver cache
  return objectCache_->deliverActive(notes);
}
#endif

/**
 * Schedules a new request via the given bucket on this cluster.
 *
 * @param notes
 * @param bucket
 *
 * This method will attempt to process the request on any of the available
 * backends if and only if the chosen bucket has enough resources currently
 *available.
 *
 * If the bucket does not allow processing further requests, due to its ceiling
 *limits,
 * the request must be enqueued in the bucket's local queue.
 *
 * If the queue has reached the queue limit already, a 503 (Service Unavailable)
 *will be
 * responded instead.
 */
void Director::schedule(RequestNotes* notes, RequestShaper::Node* bucket) {
  auto r = notes->request;
  notes->bucket = bucket;

  if (!enabled_) {
    serviceUnavailable(notes);
    return;
  }

#if defined(ENABLE_DIRECTOR_CACHE)
  if (processCacheObject(notes)) return;
#endif

  r->responseHeaders.push_back("X-Director-Bucket", bucket->name());

  if (notes->bucket->get(1)) {
    notes->tokens = 1;
    SchedulerStatus result1 = tryProcess(notes, BackendRole::Active);
    if (result1 == SchedulerStatus::Success) return;

    if (result1 == SchedulerStatus::Unavailable &&
        tryProcess(notes, BackendRole::Backup) == SchedulerStatus::Success)
      return;

    // we could not actually processes the request, so release the token we just
    // received.
    notes->bucket->put(1);
    notes->tokens = 0;

    if (result1 == SchedulerStatus::Unavailable &&
        enqueueOnUnavailable_ == false) {
      serviceUnavailable(notes);
      return;
    }
  } else if (unlikely(notes->bucket->ceil() == 0 && !enqueueOnUnavailable_)) {
    // there were no tokens available and we prefer not to enqueue and wait
    serviceUnavailable(notes);
    return;
  }

  tryEnqueue(notes);
}

/**
 * Verifies number of tries, this request has been attempted to be queued, to be
 *in valid range.
 *
 * \retval true tryCount is still below threashold, so further tries are
 *allowed.
 * \retval false tryCount exceeded limit and a 503 client response has been
 *sent. Dropped-stats have been incremented.
 */
bool Director::verifyTryCount(RequestNotes* notes) {
  if (notes->tryCount <= maxRetryCount()) return true;

  notes->request->log(Severity::info, "director %s: request failed %d times.",
                      name().c_str(), notes->tryCount);
  serviceUnavailable(notes);
  return false;
}

void Director::reschedule(RequestNotes* notes) {
  if (!verifyTryCount(notes)) return;

  SchedulerStatus result1 = tryProcess(notes, BackendRole::Active);
  if (result1 == SchedulerStatus::Success) return;

  if (result1 == SchedulerStatus::Unavailable &&
      tryProcess(notes, BackendRole::Backup) == SchedulerStatus::Success)
    return;

  tryEnqueue(notes);
}

/**
 * Finishes a request with a 503 (Service Unavailable) response message.
 */
void Director::serviceUnavailable(RequestNotes* notes, HttpStatus status) {
  auto r = notes->request;

#if defined(ENABLE_DIRECTOR_CACHE)
  if (objectCache_->deliverShadow(notes)) return;
#endif

  if (retryAfter()) {
    char value[64];
    snprintf(value, sizeof(value), "%zu", retryAfter().totalSeconds());
    r->responseHeaders.push_back("Retry-After", value);
  }

  r->status = status;
  r->finish();
  ++dropped_;
}

/**
 * Pops an enqueued request from the front of the queue and passes it to the
 *backend for serving.
 *
 * \param backend the backend to pass the dequeued request to.
 */
void Director::dequeueTo(Backend* backend) {
  if (auto notes = dequeue()) {
    notes->request->post([this, backend, notes]() {
      notes->tokens = 1;
      TRACE(notes, 1, "Dequeueing request to backend $0 @ $1",
            backend->name(), name());
      SchedulerStatus rc = backend->tryProcess(notes);
      if (rc != SchedulerStatus::Success) {
        notes->tokens = 0;
        static const char* ss[] = {"Unavailable.", "Success.", "Overloaded."};
        notes->request->log(
            Severity::error, "Dequeueing request to backend %s @ %s failed. %s",
            backend->name().c_str(), name().c_str(), ss[(size_t)rc]);
        reschedule(notes);
      } else {
        // FIXME: really here????
        verifyTryCount(notes);
      }
    });
  } else {
    WTRACE(1, "dequeueTo: queue empty.");
  }
}

/**
 * Attempts to enqueue the request, respecting limits.
 *
 * Attempts to enqueue the request on the associated bucket.
 * If enqueuing fails, it instead finishes the request with a 503 (Service
 *Unavailable).
 *
 * \retval true request could be enqueued.
 * \retval false request could not be enqueued. A 503 error response has been
 *sent out instead.
 */
bool Director::tryEnqueue(RequestNotes* rn) {
  if (rn->bucket->queued().current() < queueLimit()) {
    rn->backend = nullptr;
    rn->bucket->enqueue(rn);
    ++queued_;

    TRACE(rn, 1, "Director $0 [$1] overloaded. Enqueueing request ($2).",
          name(),
          rn->bucket->name(),
          rn->bucket->queued().current());

    return true;
  }

  TRACE(rn, 1, "director: '$0' queue limit $1 reached.", name(), queueLimit());

  serviceUnavailable(rn);

  return false;
}

RequestNotes* Director::dequeue() {
  if (auto rn = shaper()->dequeue()) {
    --queued_;
    TRACE(rn, 1, "Director $0 dequeued request ($1 pending).",
          name(), queued_.current());
    return rn;
  }
  WTRACE(1, "Director $0 dequeue() failed ($1 pending).",
         name(), queued_.current());

  return nullptr;
}

namespace xzero {
  template<> std::string StringUtil::toString(BackendRole role) {
    switch (role) {
      case BackendRole::Active:
        return "Active";
      case BackendRole::Backup:
        return "Backup";
      case BackendRole::Terminate:
        return "Terminate";
      default:
        return "UNKNOWN";
    }
  }
}

SchedulerStatus Director::tryProcess(RequestNotes* rn, BackendRole role) {
  TRACE(rn, 1, "Director.tryProcess(role: $0) tc:$1", role, rn->tryCount);

  return backends_[static_cast<size_t>(role)].schedule(rn);
}

void Director::onTimeout(RequestNotes* rn) {
  --queued_;

  rn->request->post([this, rn]() {
    rn->request->log(Severity::info, "Queued request timed out. %s %s",
                     rn->request->method.str().c_str(),
                     rn->request->path.c_str());

    Duration diff = rn->request->connection.worker().now() - rn->ctime;
    rn->request->log(Severity::info, "request time: %s", diff.str().c_str());

    serviceUnavailable(rn, HttpStatus::GatewayTimeout);
  });
}
