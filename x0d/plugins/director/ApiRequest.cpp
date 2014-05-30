/* <plugins/director/ApiRequest.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include "ApiRequest.h"
#include "Director.h"
#include "Backend.h"
#include "ObjectCache.h"
#include "HttpBackend.h"
#include "FastCgiBackend.h"

#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/Tokenizer.h>
#include <x0/Url.h>
#include <cstdlib>

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

using namespace x0;

/**
 * \class ApiRequest
 * \brief implements director's JSON API.
 *
 * An instance of this class is to serve one request.
 */

inline HttpMethod requestMethod(const x0::BufferRef& value)
{
    switch (value[0]) {
        case 'C':
            return value == "CONNECT"
                ? HttpMethod::CONNECT
                : HttpMethod::Unknown;
        case 'D':
            return value == "DELETE"
                ? HttpMethod::DELETE
                : HttpMethod::Unknown;
        case 'G':
            return value == "GET"
                ? HttpMethod::GET
                : HttpMethod::Unknown;
        case 'L':
            return value == "LOCK"
                ? HttpMethod::LOCK
                : HttpMethod::Unknown;
        case 'M':
            if (value == "MKCOL")
                return HttpMethod::MKCOL;
            else if (value == "MOVE")
                return HttpMethod::MOVE;
            else
                return HttpMethod::Unknown;
        case 'P':
            return value == "PUT"
                ? HttpMethod::PUT
                : value == "POST"
                    ? HttpMethod::POST
                    : HttpMethod::Unknown;
        case 'U':
            return value == "UNLOCK"
                ? HttpMethod::UNLOCK
                : HttpMethod::Unknown;
        default:
            return HttpMethod::Unknown;
    }
}

ApiRequest::ApiRequest(DirectorMap* directors, HttpRequest* r, const BufferRef& path) :
    directors_(directors),
    request_(r),
    method_(::requestMethod(request_->method)),
    path_(path),
    tokens_(tokenize(path_.ref(1), "/")),
    body_()
{
}

ApiRequest::~ApiRequest()
{
}

/**
 * instanciates an ApiRequest object and passes the given request to it, to actually handle the API request.
 *
 * \param directors pointer to the map of directors
 * \param r the client's request handle
 * \param path a modified version of the HTTP request path, to be used as such.
 */
bool ApiRequest::process(DirectorMap* directors, HttpRequest* r, const BufferRef& path)
{
    ApiRequest* ar = new ApiRequest(directors, r, path);
    ar->start();
    return true;
}

/**
 * Internal function, used to start processing the request.
 */
void ApiRequest::start()
{
    request_->setBodyCallback<ApiRequest, &ApiRequest::onBodyChunk>(this);
}

void ApiRequest::onBodyChunk(const BufferRef& chunk)
{
    body_ << chunk;

    if (chunk.empty()) {
        parseBody();

        if (!process()) {
            request_->log(Severity::error, "Error parsing request body.");
            if (!request_->status)
                request_->status = HttpStatus::BadRequest;

            request_->finish();
        }
    }
}

void ApiRequest::parseBody()
{
    args_ = Url::parseQuery(body_);
}

Director* ApiRequest::findDirector(const x0::BufferRef& name)
{
    auto i = directors_->find(name.str());

    if (i != directors_->end())
        return i->second;

    request_->log(Severity::error, "Director '%s' not found.", name.str().c_str());

    return nullptr;
}

bool ApiRequest::hasParam(const std::string& key) const
{
    return args_.find(key) != args_.end();
}

bool ApiRequest::loadParam(const std::string& key, bool& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    result = i->second == "true"
        || i->second == "1";

    return true;
}

bool ApiRequest::loadParam(const std::string& key, int& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    result = std::atoi(i->second.c_str());

    return true;
}

bool ApiRequest::loadParam(const std::string& key, size_t& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    result = std::atoll(i->second.c_str());

    return true;
}

bool ApiRequest::loadParam(const std::string& key, float& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    char* nptr = nullptr;
    result = strtof(i->second.c_str(), &nptr);

    return nptr == i->second.c_str() + i->second.size();
}

bool ApiRequest::loadParam(const std::string& key, TimeSpan& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    result = TimeSpan::fromSeconds(std::atoll(i->second.c_str()));

    return true;
}

bool ApiRequest::loadParam(const std::string& key, BackendRole& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    if (i->second == "active")
        result = BackendRole::Active;
    else if (i->second == "backup")
        result = BackendRole::Backup;
    else
        return false;

    return true;
}

bool ApiRequest::loadParam(const std::string& key, HealthMonitor::Mode& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    if (i->second == "paranoid")
        result = HealthMonitor::Mode::Paranoid;
    else if (i->second == "opportunistic")
        result = HealthMonitor::Mode::Opportunistic;
    else if (i->second == "lazy")
        result = HealthMonitor::Mode::Lazy;
    else
        return false;

    return true;
}

bool ApiRequest::loadParam(const std::string& key, std::string& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    result = i->second;

    return true;
}

bool ApiRequest::loadParam(const std::string& key, TransferMode& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    result = makeTransferMode(i->second);

    return true;
}

bool ApiRequest::loadParam(const std::string& key, ClientAbortAction& result)
{
    auto i = args_.find(key);
    if (i == args_.end()) {
        request_->log(Severity::error, "Request parameter '%s' not found.", key.c_str());
        return false;
    }

    Try<ClientAbortAction> t = parseClientAbortAction(i->second);
    if (t.isError()) {
        request_->log(Severity::error, "Request parameter '%s' is invalid. %s", key.c_str(), t.errorMessage());
        return false;
    }

    result = t.get();
    return true;
}

bool ApiRequest::process()
{
    switch (tokens_.size()) {
        case 3:
            if (tokens_[1] == "buckets")          // /:director_id/buckets/:bucket_id
                return processBucket();
            else if (tokens_[1] == "backends")    // /:director_id/backends/:bucket_id
                return processBackend();
            else
                return false;
        case 2:
            if (requestMethod() == HttpMethod::PUT) {
                if (tokens_[1] == "buckets")      // PUT /:director_id/buckets
                    return createBucket(findDirector(tokens_[0]));
                else if (tokens_[1] == "backends") // PUT /:director_id/backends
                    return createBackend(findDirector(tokens_[0]));
            }
            return badRequest();
        case 1:                                   // /:director_id
            return processDirector();
        case 0:                                   // /
            return processIndex();
        default:
            return false;
    }
}

// {{{ index
bool ApiRequest::processIndex()
{
    if (requestMethod() == HttpMethod::GET)
        return index();

    return false;
}

// GET /
bool ApiRequest::index()
{
    // FIXME: thread safety.
    // In order to make this method thread-safe, we must ensure that each director's
    // json-write is done from within the director's worker thread and finally
    // the reply be sent to the client from within the request's worker thread.

    Buffer result;
    JsonWriter json(result);

    json.beginObject();
    for (auto di: *directors_) {
        Director* director = di.second;
        json.name(director->name()).value(*director);
    }
    json.endObject();
    result << "\n";

    char slen[32];
    snprintf(slen, sizeof(slen), "%zu", result.size());

    request_->responseHeaders.push_back("Cache-Control", "no-cache");
    request_->responseHeaders.push_back("Content-Type", "application/json");
    request_->responseHeaders.push_back("Access-Control-Allow-Origin", "*");
    request_->responseHeaders.push_back("Content-Length", slen);
    request_->write<BufferSource>(result);
    request_->finish();

    return true;
}

// }}}
// {{{ directors
bool ApiRequest::processDirector()
{
    if (requestMethod() == HttpMethod::PUT) {
        return createDirector(tokens_[0].str());
    }

    Director* director = findDirector(tokens_[0]);
    if (!director) {
        request_->status = x0::HttpStatus::NotFound;
        request_->finish();
        return true;
    }

    switch (requestMethod()) {
        case HttpMethod::GET:
            return show(director);
        case HttpMethod::POST:
            return update(director);
        case HttpMethod::DELETE:
            return destroy(director);
        default:
            return false;
    }
}

// GET /:director
bool ApiRequest::show(Director* director)
{
    Buffer result;
    JsonWriter(result).value(*director);

    request_->status = x0::HttpStatus::Ok;
    request_->write<x0::BufferSource>(result);
    request_->finish();

    return true;
}

// POST /:director
bool ApiRequest::update(Director* director)
{
    bool enabled = director->isEnabled();
    if (hasParam("enabled") && !loadParam("enabled", enabled))
        return false;

    size_t queueLimit = director->queueLimit();
    if (hasParam("queue-limit") && !loadParam("queue-limit", queueLimit))
        return false;

    TimeSpan queueTimeout = director->queueTimeout();
    if (hasParam("queue-timeout") && !loadParam("queue-timeout", queueTimeout))
        return false;

    ClientAbortAction clientAbortAction = director->clientAbortAction();
    if (hasParam("on-client-abort") && !loadParam("on-client-abort", clientAbortAction))
        return false;

    TimeSpan retryAfter = director->retryAfter();
    if (hasParam("retry-after") && !loadParam("retry-after", retryAfter))
        return false;

    TimeSpan connectTimeout = director->connectTimeout();
    if (hasParam("connect-timeout") && !loadParam("connect-timeout", connectTimeout))
        return false;

    TimeSpan readTimeout = director->readTimeout();
    if (hasParam("read-timeout") && !loadParam("read-timeout", readTimeout))
        return false;

    TimeSpan writeTimeout = director->writeTimeout();
    if (hasParam("write-timeout") && !loadParam("write-timeout", writeTimeout))
        return false;

    TransferMode transferMode = director->transferMode();
    if (hasParam("transfer-mode") && !loadParam("transfer-mode", transferMode))
        return false;

    size_t maxRetryCount = director->maxRetryCount();
    if (hasParam("max-retry-count") && !loadParam("max-retry-count", maxRetryCount))
        return false;

    bool stickyOfflineMode = director->stickyOfflineMode();
    if (hasParam("sticky-offline-mode") && !loadParam("sticky-offline-mode", stickyOfflineMode))
        return false;

    bool allowXSendfile = director->allowXSendfile();
    if (hasParam("allow-x-sendfile") && !loadParam("allow-x-sendfile", allowXSendfile))
        return false;

    bool enqueueOnUnavailable = director->enqueueOnUnavailable();
    if (hasParam("enqueue-on-unavailable") && !loadParam("enqueue-on-unavailable", enqueueOnUnavailable))
        return false;

    std::string hcHostHeader = director->healthCheckHostHeader();
    if (hasParam("health-check-host-header") && !loadParam("health-check-host-header", hcHostHeader))
        return false;

    std::string hcRequestPath = director->healthCheckRequestPath();
    if (hasParam("health-check-request-path") && !loadParam("health-check-request-path", hcRequestPath))
        return false;

    std::string hcFcgiScriptFileName = director->healthCheckFcgiScriptFilename();
    if (hasParam("health-check-fcgi-script-filename") && !loadParam("health-check-fcgi-script-filename", hcFcgiScriptFileName))
        return false;

    std::string scheduler = director->scheduler();
    if (hasParam("scheduler") && !loadParam("scheduler", scheduler))
        return false;

#if defined(ENABLE_DIRECTOR_CACHE)
    bool cacheEnabled = director->objectCache().enabled();
    if (hasParam("cache-enabled") && !loadParam("cache-enabled", cacheEnabled))
        return false;

    bool cacheDeliverActive = director->objectCache().deliverActive();
    if (hasParam("cache-deliver-active") && !loadParam("cache-deliver-active", cacheDeliverActive))
        return false;

    bool cacheDeliverShadow = director->objectCache().deliverShadow();
    if (hasParam("cache-deliver-shadow") && !loadParam("cache-deliver-shadow", cacheDeliverShadow))
        return false;

    TimeSpan cacheDefaultTTL = director->objectCache().defaultTTL();
    if (hasParam("cache-default-ttl") && !loadParam("cache-default-ttl", cacheDefaultTTL))
        return false;

    TimeSpan cacheDefaultShadowTTL = director->objectCache().defaultShadowTTL();
    if (hasParam("cache-default-shadow-ttl") && !loadParam("cache-default-shadow-ttl", cacheDefaultShadowTTL))
        return false;
#endif

    if (!director->isMutable()) {
        request_->log(Severity::error, "director: Could not update director '%s'. Director immutable.",
            director->name().c_str());

        request_->status = x0::HttpStatus::Forbidden;
        request_->finish();
        return true;
    }

    director->setEnabled(enabled);
    director->setQueueLimit(queueLimit);
    director->setQueueTimeout(queueTimeout);
    director->setClientAbortAction(clientAbortAction);
    director->setRetryAfter(retryAfter);
    director->setConnectTimeout(connectTimeout);
    director->setReadTimeout(readTimeout);
    director->setWriteTimeout(writeTimeout);
    director->setTransferMode(transferMode);
    director->setMaxRetryCount(maxRetryCount);
    director->setStickyOfflineMode(stickyOfflineMode);
    director->setAllowXSendfile(allowXSendfile);
    director->setEnqueueOnUnavailable(enqueueOnUnavailable);
    director->setHealthCheckHostHeader(hcHostHeader);
    director->setHealthCheckRequestPath(hcRequestPath);
    director->setHealthCheckFcgiScriptFilename(hcFcgiScriptFileName);
    director->setScheduler(scheduler);

#if defined(ENABLE_DIRECTOR_CACHE)
    director->objectCache().setEnabled(cacheEnabled);
    director->objectCache().setDeliverActive(cacheDeliverActive);
    director->objectCache().setDeliverShadow(cacheDeliverShadow);
    director->objectCache().setDefaultTTL(cacheDefaultTTL);
    director->objectCache().setDefaultShadowTTL(cacheDefaultShadowTTL);
#endif

    director->save();

    director->post([director]() {
        director->eachBackend([](Backend* backend) {
            backend->healthMonitor()->update();
        });
    });

    request_->log(Severity::info, "director: %s reconfigured.", director->name().c_str());
    request_->status = x0::HttpStatus::Accepted;
    request_->finish();

    return true;
}

// PUT /:director
bool ApiRequest::createDirector(const std::string& name)
{
    return false; // TODO
}

// DELETE /:director
bool ApiRequest::destroy(Director* director)
{
    return false; // TODO
}
// }}}
// {{{ backends
bool ApiRequest::processBackend()
{
    Director* director = findDirector(tokens_[0]);
    if (!director) {
        request_->status = x0::HttpStatus::NotFound;
        request_->finish();
        return true;
    }

    switch (requestMethod()) {
        case HttpMethod::GET:
            return show(director->findBackend(tokens_[2].str()));
        case HttpMethod::POST:
            return update(director->findBackend(tokens_[2].str()), director);
        case HttpMethod::UNLOCK:
            return lock(false, director->findBackend(tokens_[2].str()), director);
        case HttpMethod::LOCK:
            return lock(true, director->findBackend(tokens_[2].str()), director);
        case HttpMethod::DELETE:
            return destroy(director->findBackend(tokens_[2].str()), director);
        default:
            return false;
    }
}

// GET /:director/:backend
bool ApiRequest::show(Backend* backend)
{
    Buffer result;
    JsonWriter json(result);
    json.beginObject()
        .value(*backend)
        .endObject();

    request_->status = x0::HttpStatus::Ok;
    request_->write<x0::BufferSource>(result);
    request_->finish();

    return true;
}

// PUT /:director_id/backends
bool ApiRequest::createBackend(Director* director)
{
    if (!director) {
        request_->status = x0::HttpStatus::NotFound;
        request_->finish();
        return true;
    }

    std::string name;
    if (!loadParam("name", name))
        return false;

    if (name.empty())
        return badRequest("Failed parsing attribute 'name'. value is empty.");

    BackendRole role = BackendRole::Active;
    if (!loadParam("role", role))
        return false;

    bool enabled = false;
    if (!loadParam("enabled", enabled))
        return false;

    size_t capacity = 0;
    if (!loadParam("capacity", capacity))
        return false;

    bool terminateProtection = false;
    if (hasParam("terminate-protection"))
        if (!loadParam("terminate-protection", terminateProtection))
            return false;

    std::string protocol;
    if (!loadParam("protocol", protocol))
        return false;

    if (protocol != "fastcgi" && protocol != "http")
        return false;

    SocketSpec socketSpec;
    std::string path;
    if (loadParam("path", path)) {
        socketSpec = SocketSpec::fromLocal(path);
    } else {
        std::string hostname;
        if (!loadParam("hostname", hostname))
            return false;

        int port;
        if (!loadParam("port", port))
            return false;

        socketSpec = SocketSpec::fromInet(IPAddress(hostname), port);
    }

    TimeSpan hcInterval;
    if (!loadParam("health-check-interval", hcInterval))
        return false;

    HealthMonitor::Mode hcMode;
    if (!loadParam("health-check-mode", hcMode))
        return false;

    if (!director->isMutable()) {
        request_->log(Severity::error, "director: Could not create backend '%s' at director '%s'. Director immutable.",
            name.c_str(), director->name().c_str());

        request_->status = x0::HttpStatus::Forbidden;
        request_->finish();
        return true;
    }

    Backend* backend = director->createBackend(name, protocol, socketSpec, capacity, role);
    if (!backend)
        return badRequest("Creating backend failed.");

    backend->setTerminateProtection(terminateProtection);
    backend->setEnabled(enabled);
    backend->healthMonitor()->setInterval(hcInterval);
    backend->healthMonitor()->setMode(hcMode);
    director->save();
    request_->status = x0::HttpStatus::Created;
    request_->log(Severity::info, "director: %s created backend: %s.", director->name().c_str(), backend->name().c_str());
    request_->finish();

    return true;
}

bool ApiRequest::update(Backend* backend, Director* director)
{
    if (!backend) {
        request_->status = x0::HttpStatus::NotFound;
        request_->finish();
        return true;
    }

    if (!director->isMutable()) {
        request_->log(Severity::error, "director: Could not update backend '%s' at director '%s'. Director immutable.",
            backend->name().c_str(), director->name().c_str());

        request_->status = x0::HttpStatus::Forbidden;
        request_->finish();
        return true;
    }

    BackendRole role = director->backendRole(backend);
    loadParam("role", role);

    bool enabled = backend->isEnabled();
    loadParam("enabled", enabled);

    size_t capacity = backend->capacity();
    loadParam("capacity", capacity);

    bool terminateProtection = backend->terminateProtection();
    if (hasParam("terminate-protection"))
        loadParam("terminate-protection", terminateProtection);

    TimeSpan hcInterval = backend->healthMonitor()->interval();
    loadParam("health-check-interval", hcInterval);

    HealthMonitor::Mode hcMode = backend->healthMonitor()->mode();
    loadParam("health-check-mode", hcMode);

    if (!enabled)
        backend->setEnabled(false);

    size_t oldCapacity = backend->capacity();
    if (oldCapacity != capacity)
        director->shaper()->resize(director->shaper()->size() - oldCapacity + capacity);

    director->setBackendRole(backend, role);
    backend->setCapacity(capacity);
    backend->setTerminateProtection(terminateProtection);
    backend->healthMonitor()->setInterval(hcInterval);
    backend->healthMonitor()->setMode(hcMode);

    if (enabled)
        backend->setEnabled(true);

    director->save();

    request_->log(Severity::info, "director: %s reconfigured backend: %s.", director->name().c_str(), backend->name().c_str());
    request_->status = x0::HttpStatus::Accepted;
    request_->finish();

    return true;
}

// LOCK or UNLOCK /:director_id/:backend_id
bool ApiRequest::lock(bool locked, Backend* backend, Director* director)
{
    backend->setEnabled(!locked);
    request_->status = x0::HttpStatus::Accepted;
    request_->finish();
    return true;
}

// delete a backend
bool ApiRequest::destroy(Backend* backend, Director* director)
{
    if (!director->isMutable()) {
        request_->log(Severity::error, "director: Could not delete backend '%s' at director '%s'. Director immutable.",
            tokens_[1].str().c_str(), tokens_[0].str().c_str());

        request_->status = x0::HttpStatus::Forbidden;
        request_->finish();
        return true;
    }

    if (backend->terminateProtection()) {
        request_->log(Severity::error, "director: Could not delete backend '%s' at director '%s'. Backend is termination protected.",
            tokens_[1].str().c_str(), tokens_[0].str().c_str());

        request_->status = x0::HttpStatus::Forbidden;
        request_->finish();
        return true;
    }

    if (director->backendRole(backend) == BackendRole::Terminate) {
        request_->log(Severity::warn, "director: trying to terminate a backend that is already initiated for termination.");
        request_->status = x0::HttpStatus::BadRequest;
        request_->finish();
        return true;
    }

    director->terminateBackend(backend);
    director->save();

    request_->log(Severity::error, "director: Deleting backend '%s' at director '%s'.",
        tokens_[1].str().c_str(), tokens_[0].str().c_str());

    request_->status = x0::HttpStatus::Accepted;
    request_->finish();

    return true;
}

// }}}
// {{{ buckets
bool ApiRequest::processBucket()
{
    // methods: GET, PUT, POST, DELETE
    // route: /:director_id/buckets/:bucket_id

    Director* director = findDirector(tokens_[0]);
    if (!director)
        return resourceNotFound("director", tokens_[0].str());

    // XXX The capture-by-value is intentional, as captures might not be reachable within the block.
    director->post([=]() {
        processBucket(director);
    });

    return true;
}

void ApiRequest::processBucket(Director* director)
{
    auto bucket = director->findBucket(tokens_[2].str());
    if (!bucket) {
        resourceNotFound("bucket", tokens_[2].str());
        return;
    }

    switch (requestMethod()) {
        case HttpMethod::GET: {
            show(bucket);
            break;
        }
        case HttpMethod::POST: { // update existing bucket
            return;
        }
        case HttpMethod::DELETE: { // delete bucket
            if (!bucket) {
                resourceNotFound("bucket", tokens_[2].str());
                return;
            }

            director->worker()->log(Severity::debug, "director %s: Destroying bucket %s",
                director->name().c_str(), bucket->name().c_str());

            director->shaper()->destroyNode(bucket);
            director->save();

            request_->post([&]() {
                request_->status = HttpStatus::Ok;
                request_->finish();
            });
            return;
        }
        default: {
            request_->post([&]() {
                request_->status = HttpStatus::BadRequest;
                request_->finish();
            });
            return;
        }
    }
}

bool ApiRequest::createBucket(Director* director)
{
    if (!director) {
        request_->status = x0::HttpStatus::NotFound;
        request_->finish();
        return true;
    }

    std::string name = tokens_[2].str();
    float rate = 0;
    float ceil = 0;

    if (!loadParam("rate", rate))
        return badRequest("invalid bucket rate");

    if (!loadParam("ceil", ceil))
        return badRequest("invalid bucket ceil");

    auto bucket = director->findBucket(tokens_[2].str());
    if (bucket) {
        // resource already exists
        request_->post([&]() {
            request_->log(Severity::notice, "Attempting to create a bucket with a name that already exists: %s.", name.c_str());
            request_->status = HttpStatus::Ok;
            request_->finish();
        });
        return true;
    }

    TokenShaperError ec = director->createBucket(name, rate, ceil);
    if (ec == TokenShaperError::Success) {
        director->save();
        request_->status = HttpStatus::Ok;
    } else {
        static const char *str[] = {
            "Success.",
            "Rate limit overflow.",
            "Ceil limit overflow.",
            "Name conflict.",
            "Invalid child node.",
        };
        director->worker()->log(Severity::error, "Could not create director's bucket. %s", str[(size_t)ec]);
        request_->status = HttpStatus::BadRequest;
    }

    request_->post([&]() {
        request_->finish();
    });
    return true;
}

void ApiRequest::show(RequestShaper::Node* bucket)
{
    Buffer result;
    JsonWriter json(result);
    bucket->writeJSON(json);
    result << "\n";

    request_->post([&]() {
        char slen[32];
        snprintf(slen, sizeof(slen), "%zu", result.size());
        request_->responseHeaders.push_back("Cache-Control", "no-cache");
        request_->responseHeaders.push_back("Content-Type", "application/json");
        request_->responseHeaders.push_back("Access-Control-Allow-Origin", "*");
        request_->responseHeaders.push_back("Content-Length", slen);
        request_->write<BufferSource>(result);
        request_->finish();
    });
}

void ApiRequest::update(RequestShaper::Node* bucket, Director* director)
{
    std::string name = tokens_[2].str();
    float rate = 0;
    float ceil = 0;

    if (!loadParam("rate", rate)) {
        request_->post([&]() {
            request_->log(Severity::error, "invalid rate");
            request_->status = HttpStatus::BadRequest;
            request_->finish();
        });
        return;
    }

    if (!loadParam("ceil", ceil)) {
        request_->post([&]() {
            request_->log(Severity::error, "invalid ceil");
            request_->status = HttpStatus::BadRequest;
            request_->finish();
        });
        return;
    }

    TokenShaperError ec = bucket->setRate(rate, ceil);

    if (ec == TokenShaperError::Success) {
        director->save();
        request_->status = HttpStatus::Ok;
    } else {
        static const char *str[] = {
            "Success.",
            "Rate limit overflow.",
            "Ceil limit overflow.",
            "Name conflict.",
            "Invalid child node.",
        };
        director->worker()->log(Severity::error, "Could not create director's bucket. %s", str[(size_t)ec]);
        request_->status = HttpStatus::BadRequest;
    }

    request_->post([&]() {
        request_->finish();
    });
}

// }}}
// {{{ helper
std::vector<x0::BufferRef> ApiRequest::tokenize(const x0::BufferRef& input, const std::string& delimiter)
{
    x0::Tokenizer<BufferRef, BufferRef> st(input, delimiter);
    return st.tokenize();
}

bool ApiRequest::resourceNotFound(const std::string& name, const std::string& value)
{
    request_->post([&]() {
        request_->log(Severity::error,
            "director: Failed to update a %s '%s'. Not found (from path: '%s').",
            name.c_str(), value.c_str(), path_.ref(1).str().c_str());

        request_->status = x0::HttpStatus::NotFound;
        request_->finish();
    });

    return true;
}

bool ApiRequest::badRequest(const char* msg)
{
    request_->post([&]() {
        if (msg && *msg)
            request_->log(Severity::error, "%s", msg);

        request_->status = x0::HttpStatus::BadRequest;
        request_->finish();
    });
    return true;
}
// }}}
