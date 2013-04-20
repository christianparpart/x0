/* <plugins/director/ApiRequest.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include "ApiRequest.h"
#include "Director.h"
#include "Backend.h"
#include "HttpBackend.h"
#include "FastCgiBackend.h"

#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>
#include <x0/Tokenizer.h>
#include <x0/Url.h>

// index:   GET    /
// get:     GET    /:director_id
//
// enable:  UNLOCK /:director_id/:backend_id
// disable: LOCK   /:director_id/:backend_id
//
// create:  PUT    /:director_id/:backend_id
// update:  POST   /:director_id/:backend_id
// delete:  DELETE /:director_id/:backend_id
//
// PUT / POST args:
// - mode
// - capacity
// - enabled

#define X_FORM_URL_ENCODED "application/x-www-form-urlencoded"

using namespace x0;

/**
 * \class ApiRequest
 * \brief implements director's JSON API.
 *
 * An instance of this class is to serve one request.
 */

enum class HttpMethod // {{{
{
	Unknown,
	// HTTP
	GET,
	PUT,
	POST,
	DELETE,
	CONNECT,
	// WebDAV
	MKCOL,
	MOVE,
	COPY,
	LOCK,
	UNLOCK,
};

HttpMethod requestMethod(const BufferRef& value)
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
// }}}

ApiRequest::ApiRequest(DirectorMap* directors, HttpRequest* r, const BufferRef& path) :
	directors_(directors),
	request_(r),
	path_(path),
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
		return false;
	}

	result = i->second == "true"
		|| i->second == "1";

	return true;
}

bool ApiRequest::loadParam(const std::string& key, int& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	result = std::atoi(i->second.c_str());

	return true;
}

bool ApiRequest::loadParam(const std::string& key, size_t& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	result = std::atoll(i->second.c_str());

	return true;
}

bool ApiRequest::loadParam(const std::string& key, TimeSpan& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	result = TimeSpan::fromMilliseconds(std::atoll(i->second.c_str()));

	return true;
}

bool ApiRequest::loadParam(const std::string& key, BackendRole& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

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
	if (i == args_.end())
		return false;

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
	if (i == args_.end())
		return false;

	result = i->second;

	return true;
}

bool ApiRequest::loadParam(const std::string& key, TransferMode& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	result = makeTransferMode(i->second);

	return true;
}

bool ApiRequest::process()
{
	switch (requestMethod(request_->method)) {
		case HttpMethod::GET:
			return path_ == "/"
				? index()
				: path_ == "/.sse"
					? eventstream()
					: get();
		case HttpMethod::UNLOCK:
			return lock(false);
		case HttpMethod::LOCK:
			return lock(true);
		case HttpMethod::PUT:
			return create() || update();
		case HttpMethod::POST:
			return update();
		case HttpMethod::DELETE:
			return destroy();
		default:
			return false;
	}
}

bool ApiRequest::index()
{
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

bool ApiRequest::eventstream()
{
	return false;
}

// get a single director json object
bool ApiRequest::get()
{
	auto tokens = tokenize(path_.ref(1), "/");
	if (tokens.size() < 1 || tokens.size() >  2)
		return false;

	request_->responseHeaders.push_back("Cache-Control", "no-cache");

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
	} else if (tokens.size() == 1) { // director
		Buffer result;
		JsonWriter(result).value(*director);

		request_->status = x0::HttpStatus::Ok;
		request_->write<x0::BufferSource>(result);
		request_->finish();
	} else { // backend
		if (Backend* backend = director->findBackend(tokens[1].str())) {
			Buffer result;
			JsonWriter json(result);
			json.beginObject()
				.value(*backend)
				.endObject();

			request_->status = x0::HttpStatus::Ok;
			request_->write<x0::BufferSource>(result);
			request_->finish();
		} else {
			request_->status = x0::HttpStatus::NotFound;
			request_->finish();
		}
	}

	return true;
}


// LOCK or UNLOCK /:director_id/:backend_id
bool ApiRequest::lock(bool locked)
{
	auto tokens = tokenize(path_.ref(1), "/");
	if (tokens.size() != 2)
		return false;

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
		return true;
	}

	// name can be passed by URI path or via request body
	auto name = tokens[1];
	if (name.empty())
		return false;

	if (Backend* backend = director->findBackend(name.str())) {
		backend->setEnabled(!locked);
		request_->status = x0::HttpStatus::Accepted;
	} else {
		request_->status = x0::HttpStatus::NotFound;
	}

	request_->finish();
	return true;
}

// create a backend - PUT /:director_id(/:backend_id)
bool ApiRequest::create()
{
	auto tokens = tokenize(path_.ref(1), "/");
	if (tokens.size() > 2)
		return false;

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
		return true;
	}

	// name can be passed by URI path or via request body
	std::string name;
	if (tokens.size() == 2)
		name = tokens[1].str();
	else if (!loadParam("name", name))
		return false;

	if (name.empty())
		return false;

	BackendRole role = BackendRole::Active;
	if (!loadParam("role", role))
		return false;

	bool enabled = false;
	if (!loadParam("enabled", enabled))
		return false;

	size_t capacity = 0;
	if (!loadParam("capacity", capacity))
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

	if (backend) {
		backend->setEnabled(enabled);
		backend->healthMonitor()->setInterval(hcInterval);
		backend->healthMonitor()->setMode(hcMode);
		director->save();
		request_->status = x0::HttpStatus::Created;
		request_->log(Severity::info, "director: %s created backend: %s.", director->name().c_str(), backend->name().c_str());
		request_->finish();
	} else {
		request_->status = x0::HttpStatus::BadRequest;
		request_->finish();
	}

	return true;
}

// update a backend - POST /:director_name/:backend_name
// allows updating of the following attributes:
// - capacity
// - enabled
// - role
// - health-check-mode
// - health-check-interval
bool ApiRequest::update()
{
	auto tokens = tokenize(path_.ref(1), "/");
	if (tokens.size() == 0 || tokens.size() > 2) {
		request_->log(Severity::error, "director: Invalid formed request path.");
		request_->status = x0::HttpStatus::BadRequest;
		request_->finish();
		return true;
	}

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->log(Severity::error,
			"director: Failed to update a resource with director '%s' not found (from path: '%s').",
			tokens[0].str().c_str(), path_.ref(1).str().c_str());
		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
		return true;
	}

	if (tokens.size() == 2)
		return updateBackend(director, tokens[1].str());
	else
		return updateDirector(director);
}

bool ApiRequest::updateDirector(Director* director)
{
	size_t queueLimit = director->queueLimit();
	if (hasParam("queue-limit") && !loadParam("queue-limit", queueLimit))
		return false;

	TimeSpan queueTimeout = director->queueTimeout();
	if (hasParam("queue-timeout") && !loadParam("queue-timeout", queueTimeout))
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

	std::string hcHostHeader = director->healthCheckHostHeader();
	if (hasParam("health-check-host-header") && !loadParam("health-check-host-header", hcHostHeader))
		return false;

	std::string hcRequestPath = director->healthCheckRequestPath();
	if (hasParam("health-check-request-path") && !loadParam("health-check-request-path", hcRequestPath))
		return false;

	std::string hcFcgiScriptFileName = director->healthCheckFcgiScriptFilename();
	if (hasParam("health-check-fcgi-script-filename") && !loadParam("health-check-fcgi-script-filename", hcFcgiScriptFileName))
		return false;

	if (!director->isMutable()) {
		request_->log(Severity::error, "director: Could not update director '%s'. Director immutable.",
			director->name().c_str());

		request_->status = x0::HttpStatus::Forbidden;
		request_->finish();
		return true;
	}

	director->setQueueLimit(queueLimit);
	director->setQueueTimeout(queueTimeout);
	director->setRetryAfter(retryAfter);
	director->setConnectTimeout(connectTimeout);
	director->setReadTimeout(readTimeout);
	director->setWriteTimeout(writeTimeout);
	director->setTransferMode(transferMode);
	director->setMaxRetryCount(maxRetryCount);
	director->setStickyOfflineMode(stickyOfflineMode);
	director->setHealthCheckHostHeader(hcHostHeader);
	director->setHealthCheckRequestPath(hcRequestPath);
	director->setHealthCheckFcgiScriptFilename(hcFcgiScriptFileName);
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

bool ApiRequest::updateBackend(Director* director, const std::string& name)
{
	if (name.empty()) {
		request_->log(Severity::error, "director: Cannot update backend with empty name.");
		return false;
	}

	Backend* backend = director->findBackend(name);
	if (!backend) {
		request_->log(Severity::error, "director: Could not update backend '%s' of director '%s'. Backend not found.",
				name.c_str(), director->name().c_str());
		return false;
	}

	BackendRole role = director->backendRole(backend);
	loadParam("role", role);

	bool enabled = backend->isEnabled();
	loadParam("enabled", enabled);

	size_t capacity = backend->capacity();
	loadParam("capacity", capacity);

	TimeSpan hcInterval = backend->healthMonitor()->interval();
	loadParam("health-check-interval", hcInterval);

	HealthMonitor::Mode hcMode = backend->healthMonitor()->mode();
	loadParam("health-check-mode", hcMode);

	if (!director->isMutable()) {
		request_->log(Severity::error, "director: Could not update backend '%s' at director '%s'. Director immutable.",
			name.c_str(), director->name().c_str());

		request_->status = x0::HttpStatus::Forbidden;
		request_->finish();
		return true;
	}

	if (!enabled)
		backend->setEnabled(false);

	size_t oldCapacity = backend->capacity();
	if (oldCapacity != capacity)
		director->shaper()->resize(director->shaper()->size() - oldCapacity + capacity);

	director->setBackendRole(backend, role);
	backend->setCapacity(capacity);
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

// delete a backend
bool ApiRequest::destroy()
{
	auto tokens = tokenize(path_.ref(1), "/");
	if (tokens.size() != 2) {
		request_->log(Severity::error, "director: Could not delete backend. Invalid request path '%s'.",
			path_.str().c_str());

		request_->status = x0::HttpStatus::InternalServerError;
		request_->finish();
		return true;
	}

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->log(Severity::error, "director: Could not delete backend '%s' at director '%s'. Director not found.",
			tokens[1].str().c_str(), tokens[0].str().c_str());

		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
		return true;
	}

	if (!director->isMutable()) {
		request_->log(Severity::error, "director: Could not delete backend '%s' at director '%s'. Director immutable.",
			tokens[1].str().c_str(), tokens[0].str().c_str());

		request_->status = x0::HttpStatus::Forbidden;
		request_->finish();
		return true;
	}

	Backend* backend = director->findBackend(tokens[1].str());
	if (!backend) {
		request_->log(Severity::error, "director: Could not delete backend '%s' at director '%s'. Backend not found.",
			tokens[1].str().c_str(), tokens[0].str().c_str());

		request_->status = x0::HttpStatus::NotFound;
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
		tokens[1].str().c_str(), tokens[0].str().c_str());

	request_->status = x0::HttpStatus::Accepted;
	request_->finish();

	return true;
}

std::vector<x0::BufferRef> ApiRequest::tokenize(const x0::BufferRef& input, const std::string& delimiter)
{
	x0::Tokenizer<BufferRef, Buffer> st(input.str(), delimiter);
	return st.tokenize();
}
