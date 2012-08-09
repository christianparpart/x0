#include "ApiRequest.h"
#include "Director.h"
#include "Backend.h"
#include "HttpBackend.h"
#include "FastCgiBackend.h"

#include <x0/StringTokenizer.h>
#include <x0/http/HttpHeader.h>
#include <x0/io/BufferSource.h>

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
 * \class ApiReqeust
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

static inline std::string urldecode(const std::string& AString) { // {{{
	Buffer sb;

    for (std::string::size_type i = 0, e = AString.size(); i < e; ++i) {
        if (AString[i] == '%') {
			std::string snum(AString.substr(++i, 2));
            ++i;
			sb.push_back(char(std::strtol(snum.c_str(), 0, 16) & 0xFF));
        } else if (AString[i] == '+')
			sb.push_back(' ');
        else
			sb.push_back(AString[i]);
    }

    return sb.str();
} // }}}

static inline std::unordered_map<std::string, std::string> parseArgs(const char *AQuery) { // {{{
	std::unordered_map<std::string, std::string> args;
	const char *data = AQuery;

	for (const char *p = data; *p; ) {
		unsigned len = 0;
		const char *q = p;

		while (*q && *q != '=' && *q != '&') {
			++q;
			++len;
		}

		if (len) {
			std::string name(p, 0, len);
			p += *q == '=' ? len + 1 : len;

			len = 0;
			for (q = p; *q && *q != '&'; ++q, ++len)
				;

			if (len) {
				std::string value(p, 0, len);
				p += len;

				for (; *p == '&'; ++p)
					; // consume '&' chars (usually just one)

				args[urldecode(name)] = urldecode(value);
			} else {
				if (*p)
					++p;

				args[urldecode(name)] = "";
			}
		} else if (*p) // && or ?& or &=
			++p;
	}
	return std::move(args);
}
// }}}

ApiReqeust::ApiReqeust(DirectorMap* directors, HttpRequest* r, const BufferRef& path) :
	directors_(directors),
	request_(r),
	path_(path),
	body_()
{
}

ApiReqeust::~ApiReqeust()
{
}

/**
 * instanciates an ApiReqeust object and passes the given request to it, to actually handle the API request.
 *
 * \param directors pointer to the map of directors
 * \param r the client's request handle
 * \param path a modified version of the HTTP request path, to be used as such.
 */
bool ApiReqeust::process(DirectorMap* directors, HttpRequest* r, const BufferRef& path)
{
	ApiReqeust* ar = new ApiReqeust(directors, r, path);
	ar->start();
	return true;
}

/**
 * Internal function, used to start processing the request.
 */
void ApiReqeust::start()
{
	request_->setBodyCallback<ApiReqeust, &ApiReqeust::onBodyChunk>(this);
}

void ApiReqeust::onBodyChunk(const BufferRef& chunk)
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

void ApiReqeust::parseBody()
{
	args_ = parseArgs(body_.c_str());
}

Director* ApiReqeust::findDirector(const std::string& name)
{
	auto i = directors_->find(name);

	if (i != directors_->end())
		return i->second;

	return nullptr;
}

bool ApiReqeust::hasParam(const std::string& key) const
{
	return args_.find(key) != args_.end();
}

bool ApiReqeust::loadParam(const std::string& key, bool& result)
{
	auto i = args_.find(key);
	if (i == args_.end()) {
		return false;
	}

	result = i->second == "true"
		|| i->second == "1";

	return true;
}

bool ApiReqeust::loadParam(const std::string& key, int& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	result = std::atoi(i->second.c_str());

	return true;
}

bool ApiReqeust::loadParam(const std::string& key, size_t& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	result = std::atoll(i->second.c_str());

	return true;
}

bool ApiReqeust::loadParam(const std::string& key, TimeSpan& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	result = TimeSpan::fromMilliseconds(std::atoll(i->second.c_str()));

	return true;
}

bool ApiReqeust::loadParam(const std::string& key, Backend::Role& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	if (i->second == "active")
		result = Backend::Role::Active;
	else if (i->second == "standby")
		result = Backend::Role::Standby;
	else if (i->second == "backup")
		result = Backend::Role::Backup;
	else
		return false;

	return true;
}

bool ApiReqeust::loadParam(const std::string& key, HealthMonitor::Mode& result)
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

bool ApiReqeust::loadParam(const std::string& key, std::string& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

	result = i->second;

	return true;
}

bool ApiReqeust::process()
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

bool ApiReqeust::index()
{
	Buffer result;

	result.push_back("{\n");
	size_t directorNum = 0;
	for (auto di: *directors_) {
		Director* director = di.second;

		if (directorNum++)
			result << ",\n";

		director->writeJSON(result);
	}
	result << "}\n";

	char slen[32];
	snprintf(slen, sizeof(slen), "%zu", result.size());

	request_->responseHeaders.push_back("Content-Type", "application/json");
	request_->responseHeaders.push_back("Access-Control-Allow-Origin", "*");
	request_->responseHeaders.push_back("Content-Length", slen);
	request_->write<BufferSource>(result);
	request_->finish();

	return true;
}

bool ApiReqeust::eventstream()
{
	return false;
}

// get a single director json object
bool ApiReqeust::get()
{
	auto tokens = tokenize(path_.ref(1).str(), "/", '\\');
	if (tokens.size() < 1 || tokens.size() >  2)
		return false;

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
	} else if (tokens.size() == 1) { // director
		Buffer result;
		result.push_back("{\n");
		director->writeJSON(result);
		result.push_back("}\n");

		request_->status = x0::HttpStatus::Ok;
		request_->write<x0::BufferSource>(result);
		request_->finish();
	} else { // backend
		if (Backend* backend = director->findBackend(tokens[1])) {
			Buffer result;
			result.push_back("{\n");
			backend->writeJSON(result);
			result.push_back("}\n");

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
bool ApiReqeust::lock(bool locked)
{
	auto tokens = tokenize(path_.ref(1).str(), "/", '\\');
	if (tokens.size() != 2)
		return false;

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
		return true;
	}

	// name can be passed by URI path or via request body
	std::string name(tokens[1]);
	if (name.empty())
		return false;

	if (Backend* backend = director->findBackend(name)) {
		backend->setEnabled(!locked);
		request_->status = x0::HttpStatus::Accepted;
	} else {
		request_->status = x0::HttpStatus::NotFound;
	}

	request_->finish();
	return true;
}

// create a backend - PUT /:director_id(/:backend_id)
bool ApiReqeust::create()
{
	auto tokens = tokenize(path_.ref(1).str(), "/", '\\');
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
		name = tokens[1];
	else if (!loadParam("name", name))
		return false;

	if (name.empty())
		return false;

	Backend::Role role;
	if (!loadParam("role", role))
		return false;

	bool enabled;
	if (!loadParam("enabled", enabled))
		return false;

	size_t capacity;
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

	Backend* backend = nullptr;
	if (protocol == "fastcgi") {
		backend = director->findBackend(name);
		if (backend)
			return false;

		backend = new FastCgiBackend(director, name, socketSpec, capacity);
		request_->status = x0::HttpStatus::Created;
	} else if (protocol == "http") {
		// protocol == "http"

		backend = director->findBackend(name);
		if (backend)
			return false;

		backend = new HttpBackend(director, name, socketSpec, capacity);
		request_->status = x0::HttpStatus::Created;
	} else {
		request_->status = x0::HttpStatus::BadRequest;
	}

	if (backend) {
		backend->setRole(role);
		backend->setEnabled(enabled);
		backend->healthMonitor().setInterval(hcInterval);
		backend->healthMonitor().setMode(hcMode);
		director->save();
	}

	request_->finish();

	request_->log(Severity::info, "director: %s created backend: %s.", director->name().c_str(), backend->name().c_str());

	return true;
}

// update a backend - POST /:director_name/:backend_name
// allows updating of the following attributes:
// - capacity
// - enabled
// - role
// - health-check-mode
// - health-check-interval
bool ApiReqeust::update()
{
	auto tokens = tokenize(path_.ref(1).str(), "/", '\\');
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
			tokens[0].c_str(), path_.ref(1).str().c_str());
		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
		return true;
	}

	if (tokens.size() == 2)
		return updateBackend(director, tokens[1]);
	else
		return updateDirector(director);
}

bool ApiReqeust::updateDirector(Director* director)
{
	size_t queueLimit = director->queueLimit();
	if (hasParam("queue-limit") && !loadParam("queue-limit", queueLimit))
		return false;

	size_t maxRetryCount = director->maxRetryCount();
	if (hasParam("max-retry-count") && !loadParam("max-retry-count", maxRetryCount))
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
	director->setMaxRetryCount(maxRetryCount);
	director->setHealthCheckHostHeader(hcHostHeader);
	director->setHealthCheckRequestPath(hcRequestPath);
	director->setHealthCheckFcgiScriptFilename(hcFcgiScriptFileName);
	director->save();

	director->worker().post([director]() {
		director->eachBackend([](Backend* backend) {
			backend->healthMonitor().update();
		});
	});

	request_->log(Severity::info, "director: %s reconfigured.", director->name().c_str());
	request_->status = x0::HttpStatus::Accepted;
	request_->finish();

	return true;
}

bool ApiReqeust::updateBackend(Director* director, const std::string& name)
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

	Backend::Role role;
	if (!loadParam("role", role))
		role = backend->role();

	bool enabled;
	if (!loadParam("enabled", enabled))
		enabled = backend->isEnabled();

	size_t capacity;
	if (!loadParam("capacity", capacity))
		capacity = backend->capacity();

	TimeSpan hcInterval;
	if (!loadParam("health-check-interval", hcInterval))
		hcInterval = backend->healthMonitor().interval();

	HealthMonitor::Mode hcMode;
	if (!loadParam("health-check-mode", hcMode))
		hcMode = backend->healthMonitor().mode();

	if (!director->isMutable()) {
		request_->log(Severity::error, "director: Could not update backend '%s' at director '%s'. Director immutable.",
			name.c_str(), director->name().c_str());

		request_->status = x0::HttpStatus::Forbidden;
		request_->finish();
		return true;
	}

	backend->setRole(role);
	backend->setEnabled(enabled);
	backend->setCapacity(capacity);
	backend->healthMonitor().setInterval(hcInterval);
	backend->healthMonitor().setMode(hcMode);
	director->save();

	request_->log(Severity::info, "director: %s reconfigured backend: %s.", director->name().c_str(), backend->name().c_str());
	request_->status = x0::HttpStatus::Accepted;
	request_->finish();

	return true;
}

// delete a backend
bool ApiReqeust::destroy()
{
	auto tokens = tokenize(path_.ref(1).str(), "/", '\\');
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
			tokens[1].c_str(), tokens[0].c_str());

		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
		return true;
	}

	if (!director->isMutable()) {
		request_->log(Severity::error, "director: Could not delete backend '%s' at director '%s'. Director immutable.",
			tokens[1].c_str(), tokens[0].c_str());

		request_->status = x0::HttpStatus::Forbidden;
		request_->finish();
		return true;
	}

	Backend* backend = director->findBackend(tokens[1]);
	if (!backend) {
		request_->log(Severity::error, "director: Could not delete backend '%s' at director '%s'. Backend not found.",
			tokens[1].c_str(), tokens[0].c_str());

		request_->status = x0::HttpStatus::NotFound;
		request_->finish();
		return true;
	}

	backend->terminate();
	director->save();

	request_->log(Severity::error, "director: Deleting backend '%s' at director '%s'.",
		tokens[1].c_str(), tokens[0].c_str());

	request_->status = x0::HttpStatus::Accepted;
	request_->finish();

	return true;
}

std::vector<std::string> ApiReqeust::tokenize(const std::string& input, const std::string& delimiter, char escapeChar, bool exclusive)
{
	x0::StringTokenizer st(input, delimiter, escapeChar, exclusive);
	return st.tokenize();
}

