#include <xzero/http/client/HttpClusterApiHandler.h>
#include <xzero/http/client/HttpClusterApi.h>
#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/JsonWriter.h>
#include <xzero/io/FileUtil.h>
#include <xzero/Uri.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>
#include <x0d/sysconfig.h>
#include <stdio.h>
#include <list>

namespace xzero {
namespace http {
namespace client {

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
// create backend:   PUT    /:director_id/backends/:backend_id
// update backend:   POST   /:director_id/backends/:backend_id
// enable backend:   UNLOCK /:director_id/backends/:backend_id
// disable backend:  LOCK   /:director_id/backends/:backend_id
// delete backend:   DELETE /:director_id/backends/:backend_id
//
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

HttpClusterApiHandler::HttpClusterApiHandler(HttpClusterApi* api,
                                             HttpRequest* request,
                                             HttpResponse* response,
                                             const BufferRef& prefix)
    : api_(api),
      request_(request),
      response_(response),
      prefix_(prefix),
      tokens_(),
      params_() {
}

HttpClusterApiHandler::~HttpClusterApiHandler() {
}

bool HttpClusterApiHandler::run() {
  if (!BufferRef(request_->path()).ibegins(prefix_))
    return false;

  Uri::ParamList params;
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
  logDebug("api", "path $0 tokens ($1): $2",
                  request_->path(),
                  tokens_.size(),
                  StringUtil::join(tokens_, ", "));

  switch (tokens_.size()) {
    case 3:
      if (tokens_[1] == "buckets")  // /:director_id/buckets/:bucket_id
        return processBucket();
      else if (tokens_[1] == "backends")  // /:director_id/backends/:bucket_id
        return processBackend();
      else
        return false;
    case 2:
      if (request_->method() == HttpMethod::PUT) {
        if (tokens_[1] == "buckets")  // PUT /:director_id/buckets
          return createBucket(api_->findCluster(tokens_[0]));
        else if (tokens_[1] == "backends")  // PUT /:director_id/backends
          return createBackend(api_->findCluster(tokens_[0]));
      }
      return badRequest("Invalid request URI");
    case 1:  // /:director_id
      processCluster();
      break;
    case 0:  // /
      processIndex();
      break;
    default:
      response_->setStatus(HttpStatus::BadRequest);
      response_->completed();
      break;
  }
  return true;
}

// {{{ cluster index
void HttpClusterApiHandler::processIndex() {
  if (request_->method() == HttpMethod::GET)
    index();
  else
    methodNotAllowed();
}

void HttpClusterApiHandler::index() {
  // FIXME: thread safety.
  // In order to make this method thread-safe, we must ensure that each
  // director's
  // json-write is done from within the director's worker thread and finally
  // the reply be sent to the client from within the request's worker thread.

  Buffer result;
  JsonWriter json(&result);

  json.beginObject();
  std::list<HttpCluster*> clusters = api_->listCluster();
  for (HttpCluster* cluster: clusters) {
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
  response_->output()->write(std::move(result));
  response_->completed();
}
// }}}
// {{{ cluster
void HttpClusterApiHandler::processCluster() {
  if (request_->method() == HttpMethod::PUT) {
    createCluster(tokens_[0]);
    return;
  }

  HttpCluster* cluster = api_->findCluster(tokens_[0]);
  if (!cluster) {
    response_->setStatus(HttpStatus::NotFound);
    response_->completed();
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
      methodNotAllowed();
      break;
  }
}

void HttpClusterApiHandler::createCluster(const std::string& name) {
  std::string path = FileUtil::joinPaths(X0D_CLUSTERDIR,
                                         name + ".cluster.conf");

  HttpCluster* cluster = api_->createCluster(name, path);

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
    logInfo("api", "cluster: $0 updated via create method.", cluster->name());
  } else {
    logInfo("api", "cluster: $0 created.", cluster->name());
  }

  response_->setStatus(status);
  response_->headers().push_back("Location", location);
  response_->completed();
}

// GET /:director
void HttpClusterApiHandler::showCluster(HttpCluster* cluster) {
  Buffer result;
  JsonWriter(&result).value(*cluster);

  response_->setStatus(HttpStatus::Ok);
  response_->addHeader("Cache-Control", "no-cache");
  response_->addHeader("Content-Type", "application/json");
  response_->addHeader("Access-Control-Allow-Origin", "*");
  response_->setContentLength(result.size());
  response_->output()->write(std::move(result));
  response_->completed();
}

void HttpClusterApiHandler::updateCluster(HttpCluster* cluster) {
  HttpStatus status = doUpdateCluster(cluster, HttpStatus::Ok);
  logInfo("api", "cluster: $0 reconfigured.", cluster->name());
  response_->setStatus(status);
  response_->completed();
}

HttpStatus HttpClusterApiHandler::doUpdateCluster(HttpCluster* cluster,
                                                  HttpStatus status) {
  if (!cluster->isMutable()) {
    logError("api", "cluster: Could not updatecluster '$0'. Director immutable.",
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

void HttpClusterApiHandler::disableCluster(HttpCluster* cluster) {
  cluster->setEnabled(false);
  cluster->saveConfiguration();
  response_->setStatus(HttpStatus::NoContent);
  response_->completed();
}

void HttpClusterApiHandler::enableCluster(HttpCluster* cluster) {
  cluster->setEnabled(true);
  cluster->saveConfiguration();
  response_->setStatus(HttpStatus::NoContent);
  response_->completed();
}

void HttpClusterApiHandler::destroyCluster(HttpCluster* cluster) {
  api_->destroyCluster(cluster->name());
  response_->setStatus(HttpStatus::NoContent);
  response_->completed();
}
// }}}
// {{{ backend 
bool HttpClusterApiHandler::processBackend() {
  return false;
}

bool HttpClusterApiHandler::createBackend(HttpCluster* cluster) {
  return false;
}
// }}}
// {{{ bucket
bool HttpClusterApiHandler::processBucket() {
  return false;
}

bool HttpClusterApiHandler::createBucket(HttpCluster* cluster) {
  return false;
}
// }}}

bool HttpClusterApiHandler::badRequest(const char* msg) {
  if (msg && *msg)
    logError("api", msg);

  response_->setStatus(HttpStatus::BadRequest);
  response_->completed();

  return true;
}

bool HttpClusterApiHandler::methodNotAllowed() {
  response_->setStatus(HttpStatus::MethodNotAllowed);
  response_->completed();

  return true;
}

bool HttpClusterApiHandler::hasParam(const std::string& key) const {
  return args_.find(key) != args_.end();
}

bool HttpClusterApiHandler::loadParam(const std::string& key, bool* result) {
  auto i = args_.find(key);
  if (i == args_.end()) {
    logError("api", "Request parameter '$0' not found.", key);
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

bool HttpClusterApiHandler::loadParam(const std::string& key, int* result) {
  auto i = args_.find(key);
  if (i == args_.end()) {
    logError("api", "Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = std::stoi(i->second);

  return true;
}

bool HttpClusterApiHandler::loadParam(const std::string& key, size_t* result) {
  auto i = args_.find(key);
  if (i == args_.end()) {
    logError("api", "Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = std::stoll(i->second);

  return true;
}

bool HttpClusterApiHandler::loadParam(const std::string& key, float* result) {
  auto i = args_.find(key);
  if (i == args_.end()) {
    logError("api", "Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  char* nptr = nullptr;
  *result = strtof(i->second.c_str(), &nptr);

  return nptr == i->second.c_str() + i->second.size();
}

bool HttpClusterApiHandler::loadParam(const std::string& key, Duration* result) {
  auto i = args_.find(key);
  if (i == args_.end()) {
    logError("api", "Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = Duration::fromMilliseconds(std::stoll(i->second));

  return true;
}

bool HttpClusterApiHandler::loadParam(const std::string& key, std::string* result) {
  auto i = args_.find(key);
  if (i == args_.end()) {
    logError("api", "Request parameter '$0' not found.", key);
    errorCount_++;
    return false;
  }

  *result = i->second;

  return true;
}

} // namespace client
} // namespace http
} // namespace xzero
