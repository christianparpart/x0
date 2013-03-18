/* <plugins/director/DirectorPlugin.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
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
 *
 *     handler director.fcgi(socket_spec);
 *     handler director.http(socket_spec);
 */

#include "DirectorPlugin.h"
#include "Director.h"
#include "Backend.h"
#include "ApiRequest.h"
#include "RequestNotes.h"
#include "RoadWarrior.h"
#include "HaproxyApi.h"

#include <x0/http/HttpPlugin.h>
#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/SocketSpec.h>
#include <x0/Url.h>
#include <x0/Types.h>

using namespace x0;

DirectorPlugin::DirectorPlugin(HttpServer& srv, const std::string& name) :
	HttpPlugin(srv, name),
	directors_(),
	roadWarrior_(),
	haproxyApi_(new HaproxyApi(&directors_))
{
	registerSetupFunction<DirectorPlugin, &DirectorPlugin::director_create>("director.create", FlowValue::VOID);
	registerSetupFunction<DirectorPlugin, &DirectorPlugin::director_load>("director.load", FlowValue::VOID);
	registerFunction<DirectorPlugin, &DirectorPlugin::director_segment>("director.segment");
	registerHandler<DirectorPlugin, &DirectorPlugin::director_pass>("director.pass");
	registerHandler<DirectorPlugin, &DirectorPlugin::director_api>("director.api");
	registerHandler<DirectorPlugin, &DirectorPlugin::director_fcgi>("director.fcgi"); // "fastcgi"
	registerHandler<DirectorPlugin, &DirectorPlugin::director_http>("director.http"); // "proxy.reverse"
	registerHandler<DirectorPlugin, &DirectorPlugin::director_haproxy_stats>("director.haproxy_stats");
	registerHandler<DirectorPlugin, &DirectorPlugin::director_haproxy_monitor>("director.haproxy_monitor");

	roadWarrior_ = new RoadWarrior(srv.selectWorker());
}

DirectorPlugin::~DirectorPlugin()
{
	for (auto director: directors_)
		delete director.second;

	delete roadWarrior_;
	delete haproxyApi_;
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

		// TODO verify that given director name is still available.
		// TODO verify that no director has been instanciated already by given path.

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

	return director->createBackend(name, Url::parse(url));
}
// }}}
// {{{ main function director.segment(string segment_id);
void DirectorPlugin::director_segment(x0::HttpRequest* r, const x0::FlowParams& args, x0::FlowValue& result)
{
	if (args.empty())
		return;

	std::string name(args[0].asString());
//	auto notes = r->setCustomData<RequestNotes>(this, worker().now(), backend);
//	auto notes = director->setupRequestNotes(r);
//	notes->bucketName = name;
}
// }}}
// {{{ handler director.pass(string director_id [, segment_id, [, string backend_id ]] );
bool DirectorPlugin::director_pass(HttpRequest* r, const FlowParams& args)
{
	Director* director = nullptr;
	std::string backendName;
	Backend* backend = nullptr;
	std::string bucketName;
	ClassfulScheduler::Bucket* bucket = nullptr;

	switch (args.size()) {
		case 3:
			if (!args[2].isString() && !args[2].isBuffer()) {
				r->log(Severity::error, "director: Invalid argument.");
				break;
			}
			backendName = args[2].asString();
			// fall though
		case 2:
			if (!args[1].isString() && !args[1].isBuffer()) {
				r->log(Severity::error, "director: Invalid argument.");
				break;
			}
			bucketName = args[1].asString();
			// fall though
		case 1: {
			if (!args[0].isString() && !args[0].isBuffer()) {
				r->log(Severity::error, "director: Invalid argument.");
				break;
			}

			std::string directorId(args[0].asString());
			auto i = directors_.find(directorId);
			if (i == directors_.end()) {
				r->log(Severity::error, "director: No director with name '%s' configured.", directorId.c_str());
				break;
			}

			director = i->second;

#if 0 // temporarily disabled
			// bucket
			if (!bucketName.empty()) {
				bucket = static_cast<ClassfulScheduler*>(director->scheduler())->findBucket(bucketName);
				if (!bucket) {
					// explicit bucket specified, but not found -> ignore.
					r->log(Severity::error, "director: Requested bucket '%s' not found in director '%s'.",
						bucketName.c_str(), directorId.c_str());
				}
			}
#endif

			// custom backend route
			if (!backendName.empty()) {
				backend = director->findBackend(backendName);

				if (!backend) {
					// explicit backend specified, but not found -> do not serve.
					r->log(Severity::error, "director: Requested backend '%s' not found.", backendName.c_str());
					r->status = x0::HttpStatus::ServiceUnavailable;
					director = nullptr;
				}
			}
			break;
		}
		case 0: {
			r->log(Severity::error, "director: No arguments passed, to director.pass().");
			break;
		}
		default: {
			r->log(Severity::error, "director: Too many arguments passed, to director.pass().");
			break;
		}
	}

	if (!director) {
		if (r->status != x0::HttpStatus::Undefined) {
			r->status = x0::HttpStatus::InternalServerError;
		}
		r->finish();
		return true;
	}

	auto notes = director->setupRequestNotes(r);
	notes->backend = backend;
	notes->bucket = bucket;

	server().log(Severity::debug, "director: passing request to %s.", director->name().c_str());
	director->schedule(r);
	return true;
}
// }}}
// {{{ handler director.api(string prefix);
bool DirectorPlugin::director_api(HttpRequest* r, const FlowParams& args)
{
	const char* prefix = args[0].toString();
	if (!r->path.begins(prefix))
		return false;

	BufferRef path(r->path.ref(strlen(prefix)));

	return ApiRequest::process(&directors_, r, path);
}
// }}}
// {{{ handler director.fcgi(socketspec);
bool DirectorPlugin::director_fcgi(HttpRequest* r, const FlowParams& args)
{
	x0::SocketSpec spec;
	spec << args;
	roadWarrior_->handleRequest(r, spec, RoadWarrior::FCGI);
	return true;
}
// }}}
// {{{ handler director.http(socketspec);
bool DirectorPlugin::director_http(HttpRequest* r, const FlowParams& args)
{
	x0::SocketSpec spec;
	spec << args;
	roadWarrior_->handleRequest(r, spec, RoadWarrior::HTTP);
	return true;
}
// }}}

// {{{ haproxy compatibility API
bool DirectorPlugin::director_haproxy_monitor(x0::HttpRequest* r, const x0::FlowParams& args)
{
	std::string prefix("/");
	if (!args.empty()) {
		if (args.size() > 1) {
			r->log(Severity::error, "director.haproxy_stats: invalid argument count");
			r->status = HttpStatus::InternalServerError;
			r->finish();
			return true;
		}
		if (!args[0].isString()) {
			r->log(Severity::error, "director.haproxy_stats: invalid argument type.");
			r->status = HttpStatus::InternalServerError;
			r->finish();
			return true;
		}
		prefix = args[0].toString();
	}

	if (!r->path.begins(prefix) && !r->unparsedUri.begins(prefix))
		return false;

	haproxyApi_->monitor(r);
	return true;
}

bool DirectorPlugin::director_haproxy_stats(x0::HttpRequest* r, const x0::FlowParams& args)
{
	std::string prefix("/");

	if (!args.empty()) {
		if (args.size() > 1) {
			r->log(Severity::error, "director.haproxy_stats: invalid argument count");
			r->status = HttpStatus::InternalServerError;
			r->finish();
			return true;
		}
		if (!args[0].isString()) {
			r->log(Severity::error, "director.haproxy_stats: invalid argument type.");
			r->status = HttpStatus::InternalServerError;
			r->finish();
			return true;
		}
		prefix = args[0].toString();
	}

	if (!r->path.begins(prefix) && !r->unparsedUri.begins(prefix))
		return false;

	haproxyApi_->stats(r, prefix);
	return true;
}
// }}}
X0_EXPORT_PLUGIN_CLASS(DirectorPlugin)
