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
      updateCluster(cluster);
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
  api_->createCluster(name, "");
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
  // TODO
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
