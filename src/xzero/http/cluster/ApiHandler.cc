// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/cluster/ApiHandler.h>
#include <xzero/http/cluster/Api.h>
#include <xzero/http/cluster/Backend.h>
#include <xzero/http/cluster/Cluster.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/JsonWriter.h>
#include <xzero/StringUtil.h>
#include <xzero/io/FileUtil.h>
#include <xzero/net/IPAddress.h>
#include <xzero/Uri.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>
#include <stdio.h>
#include <list>

namespace xzero::http::cluster {

// TODO: ENABLE_DIRECTOR_CLIENTABORT)
// TODO: ENABLE_DIRECTOR_CACHE
//
// list directors:   GET    /
//
// get director:     GET    /:director_id
// update director:  POST   /:director_id
// enable director:  LOCK   /:director_id
// disable director: UNLOCK /:director_id
// delete director:  DELETE /:director_id
// create director:  PUT    /:director_id
//
// create backend:   PUT    /:director_id/backends
// create backend:   PUT    /:director_id/backends/:backend_id
// update backend:   POST   /:director_id/backends/:backend_id
// enable backend:   UNLOCK /:director_id/backends/:backend_id
// disable backend:  LOCK   /:director_id/backends/:backend_id
// delete backend:   DELETE /:director_id/backends/:backend_id
//
// create bucket:    PUT    /:director_id/buckets
// create bucket:    PUT    /:director_id/buckets/:bucket_id
// update bucket:    POST   /:director_id/buckets/:bucket_id
// delete bucket:    DELETE /:director_id/buckets/:bucket_id
//
// PUT / POST args (backend):
// - mode
// - capacity
// - enabled
// - protected
// - role
// - health-check-mode
// - health-check-interval
//
// PUT / POST args (bucket):
// - rate
// - ceil
//
// POST args (director):
// - enabled
// - queue-limit
// - on-client-abort
// - retry-after
// - connect-timeout
// - read-timeout
// - write-timeout
// - max-retry-count
// - sticky-offline-mode
// - allow-x-sendfile
// - health-check-host-header
// - health-check-request-path
// - health-check-fcgi-script-filename
// - scheduler
//
// - cache-enabled              BOOL
// - cache-deliver-active       BOOL
// - cache-deliver-shadow       BOOL
// - cache-default-ttl          TIMESPAN
// - cache-default-shadow-ttl   TIMESPAN
//
// - hostname
// - port

#define X_FORM_URL_ENCODED "application/x-www-form-urlencoded"

ApiHandler::ApiHandler(Api* api,
                       HttpRequest* request,
                       HttpResponse* response,
                       const std::string& prefix)
    : api_(api),
      request_(request),
      response_(response),
      errorCount_(0),
      prefix_(prefix),
      tokens_(),
      params_() {
}

ApiHandler::~ApiHandler() {
}

bool ApiHandler::run() {
  if (!StringUtil::beginsWith(request_->path(), prefix_))
    return false;

  Uri::ParamList params;
  Uri::parseQueryString(request_->getContent().getBuffer(), &params);
  Uri::parseQueryString(request_->query(), &params);
  for (const auto& param: params)
    params_[param.first] = param.second;

  std::string str = request_->path().substr(prefix_.size());
  if (str.empty())
    str = "/";

  std::string pattern("/");
  size_t begin = 1;
  for (;;) {
    auto end = str.find(pattern, begin);
    if (end == std::string::npos) {
      if (begin != str.size())
        tokens_.emplace_back(str.substr(begin, end));
      break;
    } else {
      tokens_.emplace_back(str.substr(begin, end - begin));
      begin = end + pattern.length();
    }
  }
  logDebug("proxy.api: path $0 tokens ($1): $2",
           request_->path(),
           tokens_.size(),
           StringUtil::join(tokens_, ", "));

  switch (tokens_.size()) {
    case 3:
      if (tokens_[1] == "buckets")  // /:director_id/buckets/:bucket_id
        processBucket();
      else if (tokens_[1] == "backends")  // /:director_id/backends/:bucket_id
        processBackend();
      break;
    case 2:
      createBackendOrBucket();
      break;
    case 1:  // /:director_id
      processCluster();
      break;
    case 0:  // /
      processIndex();
      break;
    default:
      generateResponse(HttpStatus::BadRequest);
      break;
  }
  return true;
}

void ApiHandler::createBackendOrBucket() {
  if (request_->method() != HttpMethod::PUT) {
    generateResponse(HttpStatus::MethodNotAllowed);
    return;
  }

  Cluster* cluster = api_->findCluster(tokens_[0]);
  if (!cluster) {
    generateResponse(HttpStatus::NotFound);
    return;
  }

  std::string name;
  loadParam("name", &name);

  if (name.empty()) {
    generateResponse(HttpStatus::BadRequest);
    return;
  }

  if (tokens_[1] == "buckets") { // PUT /:director_id/buckets
    createBucket(cluster, name);
  } else if (tokens_[1] == "backends") { // PUT /:director_id/backends
    createBackend(cluster, name);
  } else {
    generateResponse(HttpStatus::BadRequest);
  }
}

// {{{ cluster index
void ApiHandler::processIndex() {
  if (request_->method() == HttpMethod::GET)
    index();
  else
    generateResponse(HttpStatus::MethodNotAllowed);
}

void ApiHandler::index() {
  // FIXME: thread safety.
  // In order to make this method thread-safe, we must ensure that each
  // director's
  // json-write is done from within the director's worker thread and finally
  // the reply be sent to the client from within the request's worker thread.

  Buffer result;
  JsonWriter json(&result);

  json.beginObject();
  std::list<Cluster*> clusters = api_->listCluster();
  for (Cluster* cluster: clusters) {
    json.name(cluster->name());
    json.value(*cluster);
  }
  json.endObject();
  result << "\n";

  response_->setStatus(HttpStatus::Ok);
  response_->addHeader("Cache-Control", "no-cache");
  response_->addHeader("Content-Type", "application/json");
  response_->addHeader("Access-Control-Allow-Origin", "*");
  response_->setContentLength(result.size());
  response_->write(std::move(result));
  response_->completed();
}
// }}}
// {{{ cluster
void ApiHandler::processCluster() {
  if (request_->method() == HttpMethod::PUT) {
    createCluster(tokens_[0]);
    return;
  }

  Cluster* cluster = api_->findCluster(tokens_[0]);
  if (!cluster) {
    generateResponse(HttpStatus::NotFound);
    return;
  }

  switch (request_->method()) {
    case HttpMethod::GET:
      showCluster(cluster);
      break;
    case HttpMethod::POST:
      updateCluster(cluster);
      break;
    case HttpMethod::LOCK:
      disableCluster(cluster);
      break;
    case HttpMethod::UNLOCK:
      enableCluster(cluster);
      break;
    case HttpMethod::DELETE:
      destroyCluster(cluster);
      break;
    default:
      generateResponse(HttpStatus::MethodNotAllowed);
      break;
  }
}

void ApiHandler::createCluster(const std::string& name) {
  std::string path = FileUtil::joinPaths(XZERO_CLUSTERDIR,
                                         name + ".cluster.conf");

  Cluster* cluster = api_->createCluster(name, path);

  bool isAlreadyPresent = FileUtil::exists(path);
  if (isAlreadyPresent) {
    cluster->setConfiguration(FileUtil::read(path).str(), path);
  }

  std::string location = request_->localAddress()->port() != 80
      ? StringUtil::format("http://$0:$1/", name,
                                            request_->localAddress()->port())
      : StringUtil::format("http://$0:$1/", name);

  HttpStatus status = doUpdateCluster(cluster, HttpStatus::Created);

  if (isAlreadyPresent) {
    logInfo("proxy.api: cluster: $0 updated via create method.", cluster->name());
  } else {
    logInfo("proxy.api: cluster: $0 created.", cluster->name());
  }

  response_->setStatus(status);
  response_->headers().push_back("Location", location);
  response_->completed();
}

// GET /:director
void ApiHandler::showCluster(Cluster* cluster) {
  Buffer result;
  JsonWriter(&result).value(*cluster);

  response_->setStatus(HttpStatus::Ok);
  response_->addHeader("Cache-Control", "no-cache");
  response_->addHeader("Content-Type", "application/json");
  response_->addHeader("Access-Control-Allow-Origin", "*");
  response_->setContentLength(result.size());
  response_->write(std::move(result));
  response_->completed();
}

void ApiHandler::updateCluster(Cluster* cluster) {
  HttpStatus status = doUpdateCluster(cluster, HttpStatus::Ok);
  logInfo("proxy.api: cluster: $0 reconfigured.", cluster->name());
  generateResponse(status);
}

HttpStatus ApiHandler::doUpdateCluster(Cluster* cluster,
                                       HttpStatus status) {
  if (!cluster->isMutable()) {
    logError("proxy.api: cluster: Could not updatecluster '$0'. Director immutable.",
             cluster->name());
    return HttpStatus::Forbidden;
  }

  // globals
  bool enabled = cluster->isEnabled();
  if (!tryLoadParamIfExists("enabled", &enabled))
    return HttpStatus::BadRequest;

  size_t queueLimit = cluster->queueLimit();
  if (!tryLoadParamIfExists("queue-limit", &queueLimit))
    return HttpStatus::BadRequest;

  Duration queueTimeout = cluster->queueTimeout();
  if (!tryLoadParamIfExists("queue-timeout", &queueTimeout))
    return HttpStatus::BadRequest;

  Duration retryAfter = cluster->retryAfter();
  if (!tryLoadParamIfExists("retry-after", &retryAfter))
    return HttpStatus::BadRequest;

  Duration connectTimeout = cluster->connectTimeout();
  if (!tryLoadParamIfExists("connect-timeout", &connectTimeout))
    return HttpStatus::BadRequest;

  Duration readTimeout = cluster->readTimeout();
  if (!tryLoadParamIfExists("read-timeout", &readTimeout))
    return HttpStatus::BadRequest;

  Duration writeTimeout = cluster->writeTimeout();
  if (!tryLoadParamIfExists("write-timeout", &writeTimeout))
    return HttpStatus::BadRequest;

  size_t maxRetryCount = cluster->maxRetryCount();
  if (!tryLoadParamIfExists("max-retry-count", &maxRetryCount))
    return HttpStatus::BadRequest;

  bool stickyOfflineMode = cluster->stickyOfflineMode();
  if (!tryLoadParamIfExists("sticky-offline-mode", &stickyOfflineMode))
    return HttpStatus::BadRequest;

  bool allowXSendfile = cluster->allowXSendfile();
  if (!tryLoadParamIfExists("allow-x-sendfile", &allowXSendfile))
    return HttpStatus::BadRequest;

  bool enqueueOnUnavailable = cluster->enqueueOnUnavailable();
  if (!tryLoadParamIfExists("enqueue-on-unavailable", &enqueueOnUnavailable))
    return HttpStatus::BadRequest;

  std::string hcHostHeader = cluster->healthCheckHostHeader();
  if (!tryLoadParamIfExists("health-check-host-header", &hcHostHeader))
    return HttpStatus::BadRequest;

  std::string hcRequestPath = cluster->healthCheckRequestPath();
  if (!tryLoadParamIfExists("health-check-request-path", &hcRequestPath))
    return HttpStatus::BadRequest;

#if defined(ENABLE_DIRECTOR_FCGI)
  std::string hcFcgiScriptFileName = cluster->healthCheckFcgiScriptFilename();
  if (!tryLoadParamIfExists("health-check-fcgi-script-filename", &hcFcgiScriptFileName))
    return HttpStatus::BadRequest;
#endif

  std::string scheduler = cluster->scheduler()->name();
  if (!tryLoadParamIfExists("scheduler", &scheduler))
    return HttpStatus::BadRequest;

#if defined(ENABLE_DIRECTOR_CACHE)
  bool cacheEnabled = cluster->objectCache().enabled();
  if (!tryLoadParamIfExists("cache-enabled", &cacheEnabled))
    return HttpStatus::BadRequest;

  bool cacheDeliverActive = cluster->objectCache().deliverActive();
  if (!tryLoadParamIfExists("cache-deliver-active", &cacheDeliverActive))
    return HttpStatus::BadRequest;

  bool cacheDeliverShadow = cluster->objectCache().deliverShadow();
  if (!tryLoadParamIfExists("cache-deliver-shadow", &cacheDeliverShadow))
    return HttpStatus::BadRequest;

  Duration cacheDefaultTTL = cluster->objectCache().defaultTTL();
  if (!tryLoadParamIfExists("cache-default-ttl", &cacheDefaultTTL))
    return HttpStatus::BadRequest;

  Duration cacheDefaultShadowTTL = cluster->objectCache().defaultShadowTTL();
  if (!tryLoadParamIfExists("cache-default-shadow-ttl", &cacheDefaultShadowTTL))
    return HttpStatus::BadRequest;
#endif

  cluster->setEnabled(enabled);
  cluster->setQueueLimit(queueLimit);
  cluster->setQueueTimeout(queueTimeout);
#if defined(ENABLE_DIRECTOR_CLIENTABORT)
  cluster->setClientAbortAction(clientAbortAction);
#endif
  cluster->setRetryAfter(retryAfter);
  cluster->setConnectTimeout(connectTimeout);
  cluster->setReadTimeout(readTimeout);
  cluster->setWriteTimeout(writeTimeout);
  cluster->setMaxRetryCount(maxRetryCount);
  cluster->setStickyOfflineMode(stickyOfflineMode);
  cluster->setAllowXSendfile(allowXSendfile);
  cluster->setEnqueueOnUnavailable(enqueueOnUnavailable);
  cluster->setHealthCheckHostHeader(hcHostHeader);
  cluster->setHealthCheckRequestPath(hcRequestPath);
#if defined(ENABLE_DIRECTOR_FCGI)
  cluster->setHealthCheckFcgiScriptFilename(hcFcgiScriptFileName);
#endif
  cluster->setScheduler(scheduler);

#if defined(ENABLE_DIRECTOR_CACHE)
  cluster->objectCache().setEnabled(cacheEnabled);
  cluster->objectCache().setDeliverActive(cacheDeliverActive);
  cluster->objectCache().setDeliverShadow(cacheDeliverShadow);
  cluster->objectCache().setDefaultTTL(cacheDefaultTTL);
  cluster->objectCache().setDefaultShadowTTL(cacheDefaultShadowTTL);
#endif

  // cluster->post([](cluster) {
  //   cluster->eachBackend([](Backend* backend) {
  //     backend->healthMonitor()->update();
  //   });
  // });

  cluster->saveConfiguration();

  return status;
}

void ApiHandler::disableCluster(Cluster* cluster) {
  cluster->setEnabled(false);
  cluster->saveConfiguration();
  generateResponse(HttpStatus::NoContent);
}

void ApiHandler::enableCluster(Cluster* cluster) {
  cluster->setEnabled(true);
  cluster->saveConfiguration();
  generateResponse(HttpStatus::NoContent);
}

void ApiHandler::destroyCluster(Cluster* cluster) {
  api_->destroyCluster(cluster->name());
  generateResponse(HttpStatus::NoContent);
}
// }}}
// {{{ backend 
void ApiHandler::processBackend() {
  // /:director_id/backends/:bucket_id
  Cluster* cluster = api_->findCluster(tokens_[0]);
  if (!cluster) {
    generateResponse(HttpStatus::NotFound);
    return;
  }

  if (request_->method() == HttpMethod::PUT) {
    createBackend(cluster, tokens_[2]);
    return;
  }

  Backend* backend = cluster->findMember(tokens_[2]);
  if (!backend) {
    generateResponse(HttpStatus::NotFound);
    return;
  }

  switch (request_->method()) {
    case HttpMethod::GET:
      showBackend(cluster, backend);
      break;
    case HttpMethod::POST:
      updateBackend(cluster, backend);
      break;
    case HttpMethod::UNLOCK:
      enableBackend(cluster, backend);
      break;
    case HttpMethod::LOCK:
      disableBackend(cluster, backend);
      break;
    case HttpMethod::DELETE:
      destroyBackend(cluster, backend);
      break;
    default:
      generateResponse(HttpStatus::NotFound);
      break;
  }
}

void ApiHandler::createBackend(Cluster* cluster,
                               const std::string& name) {
  logDebug("proxy.api: create backend '$0'", name);
  IPAddress ip;
  int port = 0;
  size_t capacity = 0;
  bool enabled = true;
  std::string protocol = "http";
  bool terminateProtection = false;
  Duration healthCheckInterval = 10_seconds;

  loadParam("host", &ip);
  loadParam("port", &port);
  tryLoadParamIfExists("capacity", &capacity);
  tryLoadParamIfExists("enabled", &enabled);
  tryLoadParamIfExists("protocol", &protocol);

  InetAddress addr(ip, port);

  if (errorCount_) {
    generateResponse(HttpStatus::BadRequest);
    return;
  }

  Backend* member = cluster->findMember(name);

  if (!member) {
    cluster->addMember(name, addr, capacity, enabled,
                       terminateProtection,
                       protocol,
                       healthCheckInterval);
    cluster->saveConfiguration();
  } else {
    // Use POST if you intend to update
  }

  generateResponse(HttpStatus::NoContent);
}

void ApiHandler::showBackend(Cluster* cluster, Backend* member) {
  Buffer result;
  JsonWriter json(&result);
  json.value(*member);
  result << "\n";

  response_->setStatus(HttpStatus::Ok);
  response_->addHeader("Cache-Control", "no-cache");
  response_->addHeader("Content-Type", "application/json");
  response_->addHeader("Access-Control-Allow-Origin", "*");
  response_->setContentLength(result.size());
  response_->write(std::move(result));
  response_->completed();
}

void ApiHandler::updateBackend(Cluster* cluster,
                               Backend* member) {
  if (!cluster->isMutable()) {
    logError("api",
             "director: Could not update backend '$0' at director '$1'. "
             "Director immutable.",
             member->name(), cluster->name());

    generateResponse(HttpStatus::Forbidden);
  }

  bool enabled = member->isEnabled();
  tryLoadParamIfExists("enabled", &enabled);

  size_t capacity = member->capacity();
  tryLoadParamIfExists("capacity", &capacity);

  bool terminateProtection = member->terminateProtection();
  tryLoadParamIfExists("terminate-protection", &terminateProtection);

  Duration hcInterval = member->healthMonitor()->interval();
  tryLoadParamIfExists("health-check-interval", &hcInterval);

  if (errorCount_ > 0) {
    generateResponse(HttpStatus::BadRequest);
    return;
  }

  // update backend

  size_t oldCapacity = member->capacity();
  if (oldCapacity != capacity) {
    cluster->shaper()->resize(
        cluster->shaper()->size() - oldCapacity + capacity);
    member->setCapacity(capacity);
  }

  member->setTerminateProtection(terminateProtection);
  member->healthMonitor()->setInterval(hcInterval);
  member->setEnabled(enabled);

  cluster->saveConfiguration();

  logInfo("proxy.api: director: $0 reconfigured backend: $1.",
          cluster->name(), member->name());

  generateResponse(HttpStatus::NoContent);
}

void ApiHandler::enableBackend(Cluster* cluster, Backend* member) {
  member->setEnabled(true);

  response_->setStatus(HttpStatus::NoContent);
  response_->addHeader("Cache-Control", "no-cache");
  response_->addHeader("Access-Control-Allow-Origin", "*");
  response_->completed();
}

void ApiHandler::disableBackend(Cluster* cluster, Backend* member) {
  member->setEnabled(false);

  response_->setStatus(HttpStatus::NoContent);
  response_->addHeader("Cache-Control", "no-cache");
  response_->addHeader("Access-Control-Allow-Origin", "*");
  response_->completed();
}

void ApiHandler::destroyBackend(Cluster* cluster, Backend* member) {
}
// }}}
// {{{ bucket
void ApiHandler::processBucket() {
  // methods: GET, PUT, POST, DELETE
  // route: /:director_id/buckets/:bucket_id

  Cluster* cluster = api_->findCluster(tokens_[0]);
  if (!cluster) {
    generateResponse(HttpStatus::NotFound);
    return;
  }

  const std::string& bucket = tokens_[2];

  switch (request_->method()) {
    case HttpMethod::PUT:
      createBucket(cluster, bucket);
      break;
    case HttpMethod::GET:
      showBucket(cluster, bucket);
      break;
    case HttpMethod::POST: // update existing bucket
      updateBucket(cluster, bucket);
      break;
    case HttpMethod::DELETE: // delete bucket
      destroyBucket(cluster, bucket);
      break;
    default:
      generateResponse(HttpStatus::MethodNotAllowed);
      break;
  }
}

void ApiHandler::destroyBucket(Cluster* cluster, const std::string& name) {
  Cluster::Bucket* bucket = cluster->findBucket(name);
  if (!bucket) {
    generateResponse(HttpStatus::NotFound);
    return;
  }

  logInfo("proxy.api: director $0: Destroying bucket $1", cluster->name(), name);

  cluster->shaper()->destroyNode(bucket);
  cluster->saveConfiguration();

  generateResponse(HttpStatus::NoContent);
}

void ApiHandler::createBucket(Cluster* cluster, const std::string& name) {
  float rate;
  if (!loadParam("rate", &rate)) {
    generateResponse(HttpStatus::BadRequest, "Invalid bucket rate");
    return;
  }

  float ceil;
  if (!loadParam("ceil", &ceil)) {
    generateResponse(HttpStatus::BadRequest, "Invalid bucket ceil");
    return;
  }

  Cluster::Bucket* bucket = cluster->findBucket(name);
  TokenShaperError ec = (bucket == nullptr)
                        ? cluster->createBucket(name, rate, ceil)
                        : bucket->setRate(rate, ceil);

  if (ec == TokenShaperError::Success) {
    generateResponse(HttpStatus::NoContent);
  } else {
    generateResponse(HttpStatus::BadRequest, to_string(ec));
  }
}

void ApiHandler::updateBucket(Cluster* cluster, const std::string& name) {
  float rate;
  if (!loadParam("rate", &rate)) {
    generateResponse(HttpStatus::BadRequest, "Invalid bucket rate");
    return;
  }

  float ceil;
  if (!loadParam("ceil", &ceil)) {
    generateResponse(HttpStatus::BadRequest, "Invalid bucket ceil");
    return;
  }

  Cluster::Bucket* bucket = cluster->findBucket(name);
  if (!bucket) {
    generateResponse(HttpStatus::NotFound);
    return;
  }

  TokenShaperError ec = bucket->setRate(rate, ceil);

  if (ec == TokenShaperError::Success) {
    generateResponse(HttpStatus::NoContent);
  } else {
    generateResponse(HttpStatus::BadRequest, to_string(ec));
  }
}

void ApiHandler::showBucket(Cluster* cluster, const std::string& name) {
  Cluster::Bucket* bucket = cluster->findBucket(name);
  if (!bucket) {
    generateResponse(HttpStatus::NotFound);
    return;
  }

  Buffer result;
  JsonWriter(&result).value(*bucket);

  response_->setStatus(HttpStatus::Ok);
  response_->addHeader("Cache-Control", "no-cache");
  response_->addHeader("Content-Type", "application/json");
  response_->addHeader("Access-Control-Allow-Origin", "*");
  response_->setContentLength(result.size());
  response_->write(std::move(result));
  response_->completed();
}
// }}}
// {{{ response generator helper
template<typename... Args>
bool ApiHandler::generateResponse(HttpStatus status,
                                  const std::string& msg,
                                  Args... args) {
  if (!msg.empty())
    logError("api", msg);

  response_->setStatus(status);
  response_->completed();

  return true;
}

bool ApiHandler::generateResponse(HttpStatus status) {
  response_->setStatus(status);
  response_->completed();

  return true;
}
// }}}
// {{{ parameter loading
bool ApiHandler::hasParam(const std::string& key) const {
  return params_.find(key) != params_.end();
}

bool ApiHandler::loadParam(const std::string& key, bool* result) {
  auto i = params_.find(key);
  if (i == params_.end()) {
    logError("proxy.api: Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  if (i->second == "true" || i->second == "1") {
    *result = true;
    return true;
  }

  if (i->second == "false" || i->second == "0") {
    *result = false;
    return true;
  }

  logError("api",
           "Request parameter '$0' contains an invalid value.",
           key);
  errorCount_++;
  return false;
}

bool ApiHandler::loadParam(const std::string& key, int* result) {
  auto i = params_.find(key);
  if (i == params_.end()) {
    logError("proxy.api: Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = std::stoi(i->second);

  return true;
}

bool ApiHandler::loadParam(const std::string& key, size_t* result) {
  auto i = params_.find(key);
  if (i == params_.end()) {
    logError("proxy.api: Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = std::stoll(i->second);

  return true;
}

bool ApiHandler::loadParam(const std::string& key, float* result) {
  auto i = params_.find(key);
  if (i == params_.end()) {
    logError("proxy.api: Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  char* nptr = nullptr;
  *result = strtof(i->second.c_str(), &nptr);

  return nptr == i->second.c_str() + i->second.size();
}

bool ApiHandler::loadParam(const std::string& key, Duration* result) {
  auto i = params_.find(key);
  if (i == params_.end()) {
    logError("proxy.api: Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = Duration::fromMilliseconds(std::stoll(i->second));

  return true;
}

bool ApiHandler::loadParam(const std::string& key, std::string* result) {
  auto i = params_.find(key);
  if (i == params_.end()) {
    logError("proxy.api: Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = i->second;

  return true;
}

bool ApiHandler::loadParam(const std::string& key, IPAddress* result) {
  auto i = params_.find(key);
  if (i == params_.end()) {
    logError("proxy.api: Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = IPAddress(i->second);

  return true;
}
// }}}

} // namespace xzero::http::cluster
