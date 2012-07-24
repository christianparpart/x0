#include "ApiRequest.h"
#include "Director.h"
#include "HttpBackend.h"
#include "Backend.h"

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

bool ApiReqeust::process(DirectorMap* directors, HttpRequest* r, const BufferRef& path)
{
	ApiReqeust* ar = new ApiReqeust(directors, r, path);
	ar->start();
	return true;
}

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
			request_->status = HttpError::BadRequest;
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

bool ApiReqeust::loadParam(const std::string& key, bool& result)
{
	auto i = args_.find(key);
	if (i == args_.end())
		return false;

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

		result << "\"" << director->name() << "\": {\n"
			   << "  \"load\": " << director->load() << ",\n"
			   << "  \"queued\": " << director->queued() << ",\n"
			   << "  \"mutable\": " << (director->isMutable() ? "true" : "false") << ",\n"
			   << "  \"members\": [";

		size_t backendNum = 0;
		for (auto backend: director->backends()) {
			if (backendNum++)
				result << ", ";

			result << "\n    {";
			backend->writeJSON(result);
			result << "}";
		}

		result << "\n  ]\n}\n";
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
	return false;
}


// LOCK or UNLOCK /:director_id/:backend_id
bool ApiReqeust::lock(bool locked)
{
	auto tokens = tokenize(path_.ref(1).str(), "/", '\\');
	if (tokens.size() != 2)
		return false;

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->status = x0::HttpError::NotFound;
		request_->finish();
		return true;
	}

	// name can be passed by URI path or via request body
	std::string name(tokens[1]);
	if (name.empty())
		return false;

	if (Backend* backend = director->findBackend(name)) {
		backend->setEnabled(!locked);
		request_->status = x0::HttpError::Accepted;
	} else {
		request_->status = x0::HttpError::NotFound;
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
		request_->status = x0::HttpError::NotFound;
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

	std::string hostname;
	if (!loadParam("hostname", hostname))
		return false;

	int port;
	if (!loadParam("port", port))
		return false;

	int hcInterval;
	if (!loadParam("health-check-interval", hcInterval))
		return false;

	HealthMonitor::Mode hcMode;
	if (!loadParam("health-check-mode", hcMode))
		return false;

	if (!director->isMutable()) {
		request_->status = x0::HttpError::MethodNotAllowed;
		request_->finish();
		return true;
	}

	Backend* backend = nullptr;
	if (protocol == "fastcgi") {
		// TODO fastcgi creation
		request_->status = x0::HttpError::NotImplemented;
	} else {
		// protocol == "http"

		backend = director->findBackend(name);
		if (backend)
			return false;

		backend = new HttpBackend(director, name, capacity, hostname, port);
		request_->status = x0::HttpError::Created;
	}

	if (backend) {
		backend->setRole(role);
		backend->setEnabled(enabled);
		backend->healthMonitor().setInterval(TimeSpan::fromSeconds(hcInterval));
		backend->healthMonitor().setMode(hcMode);
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
	if (tokens.size() > 2)
		return false;

	Director* director = findDirector(tokens[0]);
	if (!director) {
		request_->status = x0::HttpError::NotFound;
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

	Backend* backend = director->findBackend(name);
	if (!backend)
		return false;

	int hcInterval;
	if (!loadParam("health-check-interval", hcInterval))
		return false;

	HealthMonitor::Mode hcMode;
	if (!loadParam("health-check-mode", hcMode))
		return false;

	if (!director->isMutable()) {
		request_->status = x0::HttpError::MethodNotAllowed;
		request_->finish();
		return true;
	}

	backend->setRole(role);
	backend->setEnabled(enabled);
	backend->setCapacity(capacity);
	backend->healthMonitor().setInterval(TimeSpan::fromSeconds(hcInterval));
	backend->healthMonitor().setMode(hcMode);

	request_->status = x0::HttpError::Accepted;
	request_->finish();

	request_->log(Severity::info, "director: %s reconfigured backend: %s.", director->name().c_str(), backend->name().c_str());

	return true;
}

// delete a backend
bool ApiReqeust::destroy()
{
	return false;
}

std::vector<std::string> ApiReqeust::tokenize(const std::string& input, const std::string& delimiter, char escapeChar, bool exclusive)
{
	x0::StringTokenizer st(input, delimiter, escapeChar, exclusive);
	return st.tokenize();
}

