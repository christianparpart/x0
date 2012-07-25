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

#include "DirectorPlugin.h"
#include "Director.h"
#include "Backend.h"
#include "ApiRequest.h"

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/Types.h>

using namespace x0;

DirectorPlugin::DirectorPlugin(HttpServer& srv, const std::string& name) :
	HttpPlugin(srv, name),
	directors_()
{
	registerSetupFunction<DirectorPlugin, &DirectorPlugin::director_create>("director.create", FlowValue::VOID);
	registerSetupFunction<DirectorPlugin, &DirectorPlugin::director_load>("director.load", FlowValue::VOID);
	registerHandler<DirectorPlugin, &DirectorPlugin::director_pass>("director.pass");
	registerHandler<DirectorPlugin, &DirectorPlugin::director_api>("director.api");
}

DirectorPlugin::~DirectorPlugin()
{
	for (auto director: directors_)
		delete director.second;
}

// {{{ setup_function director.load(...)
void DirectorPlugin::director_load(const FlowParams& args, FlowValue& result)
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

		Director* director = new Director(server().nextWorker(), directorName.toString());
		if (!director)
			continue;

		director->load(path.toString());

		directors_[directorName.toString()] = director;
	}
}
// }}}

// {{{ setup_function director.create(...)
void DirectorPlugin::director_create(const FlowParams& args, FlowValue& result)
{
	const FlowValue& directorId = args[0];
	if (!directorId.isString())
		return;

	Director* director = createDirector(directorId.toString());
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

Director* DirectorPlugin::createDirector(const char* id)
{
	server().log(Severity::debug, "director: Creating director %s", id);
	Director* director = new Director(server().nextWorker(), id);
	return director;
}

Backend* DirectorPlugin::registerBackend(Director* director, const char* name, const char* url)
{
	server().log(Severity::debug, "director: %s, backend %s: %s",
			director->name().c_str(), name, url);

	return director->createBackend(name, url);
}
// }}}

// {{{ handler director.pass(string director_id [, string backend_id] );
bool DirectorPlugin::director_pass(HttpRequest* r, const FlowParams& args)
{
	Director* director = selectDirector(r, args);
	if (!director)
		return false;

	server().log(Severity::debug, "director: passing request to %s.", director->name().c_str());
	director->schedule(r);
	return true;
}

Director* DirectorPlugin::selectDirector(HttpRequest* r, const FlowParams& args)
{
	switch (args.size()) {
		case 0: {
			if (directors_.size() != 1) {
				r->log(Severity::error, "director: No directors configured.");
				return nullptr;
			}
			return directors_.begin()->second;
		}
		case 1:
		case 2: {
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

			Director* director = i->second;

			// builtin jail: support custom routing
			if (args.size() == 2) {
				std::string backendId(args[1].toString(), args[1].toString() + args[1].toNumber());
				if (Backend* backend = director->findBackend(backendId.c_str())) {
					r->setCustomData<DirectorNotes>(director);
					auto notes = r->customData<DirectorNotes>(director);
					notes->backend = backend;
				}
			}

			return director;
		}
		default: {
			r->log(Severity::error, "director: Too many arguments passed, to director.pass().");
			return nullptr;
		}
	}
}
// }}}

// {{{ handler director.api(string prefix);
bool DirectorPlugin::director_api(HttpRequest* r, const FlowParams& args)
{
	const char* prefix = args[0].toString();
	if (!r->path.begins(prefix))
		return false;

	BufferRef path(r->path.ref(strlen(prefix)));

	return ApiReqeust::process(&directors_, r, path);
}
// }}}

X0_EXPORT_PLUGIN_CLASS(DirectorPlugin)
