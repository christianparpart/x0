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
 *     function director.create(string title, string key,
 *                              string backend_name_1 => string backend_url_1,
 *                              ...);
 *
 * request processing API:
 *     handler director.pass(key);
 */

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/http/HttpHeader.h>
#include <x0/http/HttpDirector.h>
#include <x0/http/HttpBackend.h>
#include <x0/Types.h>

using namespace x0;

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
		registerHandler<DirectorPlugin, &DirectorPlugin::director_pass>("director.pass");
	}

	~DirectorPlugin()
	{
		for (auto director: directors_)
			delete director.second;
	}

private:
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

	// handler director.pass(string director_id);
	bool director_pass(HttpRequest* r, const FlowParams& args)
	{
		HttpDirector* director = selectDirector(r, args);
		if (!director)
			return false;

		server().log(Severity::debug, "director: passing request to %s.", director->name().c_str());
		director->enqueue(r);
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

	HttpDirector* createDirector(const char* id)
	{
		server().log(Severity::debug, "director: Creating director %s", id);
		HttpDirector* director = new HttpDirector(id);
		return director;
	}

	HttpBackend* registerBackend(HttpDirector* director, const char* name, const char* url)
	{
		server().log(Severity::debug, "director: %s, backend %s: %s",
				director->name().c_str(), name, url);

		return director->createBackend(name, url);
	}
}; // }}}

X0_EXPORT_PLUGIN_CLASS(DirectorPlugin)
