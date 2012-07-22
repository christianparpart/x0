/* <x0/plugins/director.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 *
 * --------------------------------------------------------------------------
 *
 * plugin type: content generator
 *
 * description:
 *     Implements basic load balancing, ideally taking out the need of an HAproxy
 *     in front of us.
 *
 * setup API:
 *     function director.create(string director_name,
 *                              string backend_name_1 => string backend_url_1,
 *                              ...);
 *
 *     function director.load(string director_name_1 => string path_to_db,
 *                            ...);
 *
 * request processing API:
 *     handler director.pass(string director_name);
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/http/HttpDirector.h>
#include <x0/http/HttpBackend.h>
#include <x0/io/BufferSource.h>
#include <x0/Types.h>

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

class DirectorPlugin : // {{{
	public HttpPlugin
{
private:
	std::unordered_map<std::string, HttpDirector*> directors_;

public:
	DirectorPlugin(HttpServer& srv, const std::string& name) :
		HttpPlugin(srv, name)
	{
		registerSetupFunction<DirectorPlugin, &DirectorPlugin::director_create>("director.create", FlowValue::VOID);
		registerSetupFunction<DirectorPlugin, &DirectorPlugin::director_load>("director.load", FlowValue::VOID);
		registerHandler<DirectorPlugin, &DirectorPlugin::director_pass>("director.pass");
		registerHandler<DirectorPlugin, &DirectorPlugin::director_api>("director.api");
	}

	~DirectorPlugin()
	{
		for (auto director: directors_)
			delete director.second;
	}

private:
	// {{{ setup_function director.load(...)
	void director_load(const FlowParams& args, FlowValue& result)
	{
		for (auto& arg: args) {
			if (!arg.isArray())
				continue;

			const FlowArray& fa = arg.toArray();
			if (fa.size() != 2)
				continue;

			const FlowValue& directorName = fa[0];
			if (!directorName.isString())
				continue;

			const FlowValue& path = fa[1];
			if (!path.isString())
				continue;

			server().log(Severity::debug, "director: Loading director %s from %s.",
				directorName.toString(), path.toString());

			HttpDirector* director = new HttpDirector(server().nextWorker(), directorName.toString());
			if (!director)
				continue;

			director->load(path.toString());

			directors_[directorName.toString()] = director;
		}
	}
	// }}}

	// {{{ setup_function director.create(...)
	void director_create(const FlowParams& args, FlowValue& result)
	{
		const FlowValue& directorId = args[0];
		if (!directorId.isString())
			return;

		HttpDirector* director = createDirector(directorId.toString());
		if (!director)
			return;

		for (auto& arg: args.shift()) {
			if (!arg.isArray())
				continue;

			const FlowArray& fa = arg.toArray();
			if (fa.size() != 2)
				continue;

			const FlowValue& backendName = fa[0];
			if (!backendName.isString())
				continue;

			const FlowValue& backendUrl = fa[1];
			if (!backendUrl.isString())
				continue;

			registerBackend(director, backendName.toString(), backendUrl.toString());
		}

		directors_[director->name()] = director;
	}

	HttpDirector* createDirector(const char* id)
	{
		server().log(Severity::debug, "director: Creating director %s", id);
		HttpDirector* director = new HttpDirector(server().nextWorker(), id);
		return director;
	}

	HttpBackend* registerBackend(HttpDirector* director, const char* name, const char* url)
	{
		server().log(Severity::debug, "director: %s, backend %s: %s",
				director->name().c_str(), name, url);

		return director->createBackend(name, url);
	}
	// }}}

	// {{{ handler director.pass(string director_id);
	bool director_pass(HttpRequest* r, const FlowParams& args)
	{
		HttpDirector* director = selectDirector(r, args);
		if (!director)
			return false;

		server().log(Severity::debug, "director: passing request to %s.", director->name().c_str());
		director->schedule(r);
		return true;
	}

	HttpDirector* selectDirector(HttpRequest* r, const FlowParams& args)
	{
		switch (args.size()) {
			case 0: {
				if (directors_.size() != 1) {
					r->log(Severity::error, "director: No directors configured.");
					return nullptr;
				}
				return directors_.begin()->second;
			}
			case 1: {
				if (!args[0].isString()) {
					r->log(Severity::error, "director: Passed director configured.");
					return nullptr;
				}
				const char* directorId = args[0].toString();
				auto i = directors_.find(directorId);
				if (i == directors_.end()) {
					r->log(Severity::error, "director: No director with name '%s' configured.", directorId);
					return nullptr;
				}
				return i->second;
			}
			default: {
				r->log(Severity::error, "director: Too many arguments passed, to director.pass().");
				return nullptr;
			}
		}
	}
	// }}}

	// {{{ handler director.api(string prefix);
	// index:   GET    /
	// get:     GET    /:director_id
	// enable:  UNLOCK /:director_id/:backend_id
	// disable: LOCK   /:director_id/:backend_id
	bool director_api(HttpRequest* r, const FlowParams& args)
	{
		const char* prefix = args[0].toString();
		if (!r->path.begins(prefix))
			return false;

		BufferRef path(r->path.ref(strlen(prefix)));
		r->log(Severity::debug5, "path: '%s'", path.str().c_str());

		switch (requestMethod(r->method)) {
			case HttpMethod::GET:
				return path == "/"
					? api_index(r)
					: path == "/.sse"
						? api_eventstream(r)
						: api_get(r, path);
			case HttpMethod::UNLOCK:
				return api_unlock(r, path);
			case HttpMethod::LOCK:
				return api_lock(r, path);
			default:
				return false;
		}
	}

	bool api_index(HttpRequest* r)
	{
		Buffer result;

		result.push_back("{\n");
		size_t directorNum = 0;
		for (auto di: directors_) {
			HttpDirector* director = di.second;

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

		r->responseHeaders.push_back("Content-Type", "application/json");
		r->responseHeaders.push_back("Access-Control-Allow-Origin", "*");
		r->responseHeaders.push_back("Content-Length", slen);
		r->write<BufferSource>(result);
		r->finish();

		return true;
	}

	bool api_eventstream(HttpRequest* r)
	{
		return false;
	}

	bool api_get(HttpRequest* r, const BufferRef& path)
	{
		return false;
	}

	bool api_lock(HttpRequest* r, const BufferRef& path)
	{
		return false;
	}

	bool api_unlock(HttpRequest* r, const BufferRef& path)
	{
		return false;
	}
	// }}}
}; // }}}

X0_EXPORT_PLUGIN_CLASS(DirectorPlugin)
