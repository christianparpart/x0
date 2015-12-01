#include <xzero/http/client/HttpClusterApiHandler.h>
#include <xzero/http/client/HttpClusterApi.h>
#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterMember.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/JsonWriter.h>
#include <xzero/Uri.h>
#include <xzero/Buffer.h>
#include <xzero/logging.h>
#include <stdio.h>
#include <list>

namespace xzero {
namespace http {
namespace client {

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
      updateCluster(cluster, HttpStatus::Ok);
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

  if (FileUtil::exists(path)) {
    logInfo("proxy", "Loading cluster $0 ($1)", name, path);
    cluster->setConfiguration(FileUtil::read(path).str(), path);
  }

  updateCluster(cluster, HttpStatus::Created);
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

void HttpClusterApiHandler::updateCluster(HttpCluster* cluster,
                                          HttpStatus status) {

  // globals
  bool enabled = cluster->isEnabled();
  if (hasParam("enabled") && !loadParam("enabled", enabled)) return false;

  size_t queueLimit = cluster->queueLimit();
  if (hasParam("queue-limit") && !loadParam("queue-limit", queueLimit))
    return false;

  Duration queueTimeout = cluster->queueTimeout();
  if (hasParam("queue-timeout") && !loadParam("queue-timeout", queueTimeout))
    return false;

  ClientAbortAction clientAbortAction = cluster->clientAbortAction();
  if (hasParam("on-client-abort") &&
      !loadParam("on-client-abort", clientAbortAction))
    return false;

  Duration retryAfter = cluster->retryAfter();
  if (hasParam("retry-after") && !loadParam("retry-after", retryAfter))
    return false;

  Duration connectTimeout = cluster->connectTimeout();
  if (hasParam("connect-timeout") &&
      !loadParam("connect-timeout", connectTimeout))
    return false;

  Duration readTimeout = cluster->readTimeout();
  if (hasParam("read-timeout") && !loadParam("read-timeout", readTimeout))
    return false;

  Duration writeTimeout = cluster->writeTimeout();
  if (hasParam("write-timeout") && !loadParam("write-timeout", writeTimeout))
    return false;

  size_t maxRetryCount = cluster->maxRetryCount();
  if (hasParam("max-retry-count") &&
      !loadParam("max-retry-count", maxRetryCount))
    return false;

  bool stickyOfflineMode = cluster->stickyOfflineMode();
  if (hasParam("sticky-offline-mode") &&
      !loadParam("sticky-offline-mode", stickyOfflineMode))
    return false;

  bool allowXSendfile = cluster->allowXSendfile();
  if (hasParam("allow-x-sendfile") &&
      !loadParam("allow-x-sendfile", allowXSendfile))
    return false;

  bool enqueueOnUnavailable = cluster->enqueueOnUnavailable();
  if (hasParam("enqueue-on-unavailable") &&
      !loadParam("enqueue-on-unavailable", enqueueOnUnavailable))
    return false;

  std::string hcHostHeader = cluster->healthCheckHostHeader();
  if (hasParam("health-check-host-header") &&
      !loadParam("health-check-host-header", hcHostHeader))
    return false;

  std::string hcRequestPath = cluster->healthCheckRequestPath();
  if (hasParam("health-check-request-path") &&
      !loadParam("health-check-request-path", hcRequestPath))
    return false;

  std::string hcFcgiScriptFileName = cluster->healthCheckFcgiScriptFilename();
  if (hasParam("health-check-fcgi-script-filename") &&
      !loadParam("health-check-fcgi-script-filename", hcFcgiScriptFileName))
    return false;

  std::string scheduler = cluster->scheduler();
  if (hasParam("scheduler") && !loadParam("scheduler", scheduler)) return false;

#if defined(ENABLE_DIRECTOR_CACHE)
  bool cacheEnabled = cluster->objectCache().enabled();
  if (hasParam("cache-enabled") && !loadParam("cache-enabled", cacheEnabled))
    return false;

  bool cacheDeliverActive = cluster->objectCache().deliverActive();
  if (hasParam("cache-deliver-active") &&
      !loadParam("cache-deliver-active", cacheDeliverActive))
    return false;

  bool cacheDeliverShadow = cluster->objectCache().deliverShadow();
  if (hasParam("cache-deliver-shadow") &&
      !loadParam("cache-deliver-shadow", cacheDeliverShadow))
    return false;

  Duration cacheDefaultTTL = cluster->objectCache().defaultTTL();
  if (hasParam("cache-default-ttl") &&
      !loadParam("cache-default-ttl", cacheDefaultTTL))
    return false;

  Duration cacheDefaultShadowTTL = cluster->objectCache().defaultShadowTTL();
  if (hasParam("cache-default-shadow-ttl") &&
      !loadParam("cache-default-shadow-ttl", cacheDefaultShadowTTL))
    return false;
#endif

  if (!cluster->isMutable()) {
    request_->log(
        Severity::error,
        "cluster: Could not updatecluster  '%s'. Director immutable.",
        cluster->name().c_str());

    request_->status = HttpStatus::Forbidden;
    request_->finish();
    return true;
  }

  cluster->setEnabled(enabled);
  cluster->setQueueLimit(queueLimit);
  cluster->setQueueTimeout(queueTimeout);
  cluster->setClientAbortAction(clientAbortAction);
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
  cluster->setHealthCheckFcgiScriptFilename(hcFcgiScriptFileName);
  cluster->setScheduler(scheduler);

#if defined(ENABLE_DIRECTOR_CACHE)
  cluster->objectCache().setEnabled(cacheEnabled);
  cluster->objectCache().setDeliverActive(cacheDeliverActive);
  cluster->objectCache().setDeliverShadow(cacheDeliverShadow);
  cluster->objectCache().setDefaultTTL(cacheDefaultTTL);
  cluster->objectCache().setDefaultShadowTTL(cacheDefaultShadowTTL);
#endif

  cluster->save();

  cluster->post([](cluster) {
    cluster->eachBackend([](Backend* backend) {
      backend->healthMonitor()->update();
    });
  });

  request_->log(Severity::info, "cluster: %s reconfigured.",
                cluster->name().c_str());

  // health checks
  // TODO
  cluster->setHealthCheckHostHeader("localhost");
  cluster->setHealthCheckRequestPath("/");
  cluster->setHealthCheckSuccessCodes({HttpStatus::Ok});
  cluster->setHealthCheckSuccessThreshold(1);
  cluster->setHealthCheckInterval(10_seconds);

  response_->setStatus(status);
  response_->completed();
}

void HttpClusterApiHandler::destroyCluster(HttpCluster* cluster) {
  // TODO
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

} // namespace client
} // namespace http
} // namespace xzero
