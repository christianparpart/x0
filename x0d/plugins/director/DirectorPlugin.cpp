/* <x0d/plugins/director/DirectorPlugin.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
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
 *
 *     function director.load(string director_name_1 => string path_to_db,
 *                            ...);
 *
 *     function director.cache.deliver_stale
 *     function director.cache.deliver_active
 *
 * request processing API:
 *     handler director.balance(string director, string bucket = "");
 *     handler director.pass(string director, string backend);
 *
 *     handler director.fcgi(socket_spec);
 *     handler director.http(socket_spec);
 *
 *     handler director.ondemand();
 *
 *     function director.cache(bool enabled);
 *     function director.cache.ttl(timespan ttl, timespan shadow_ttl = 0);
 *     function director.cache.key(string pattern);
 *     function director.cache.bypass();
 */

#include "DirectorPlugin.h"
#include "Director.h"
#include "Backend.h"
#include "ApiRequest.h"
#include "RequestNotes.h"
#include "RoadWarrior.h"
#include "HaproxyApi.h"
#include "ObjectCache.h"

#include <x0/http/HttpServer.h>
#include <x0/http/HttpRequest.h>
#include <x0/SocketSpec.h>
#include <x0/Url.h>
#include <x0/Types.h>

using namespace x0;

DirectorPlugin::DirectorPlugin(x0d::XzeroDaemon* d, const std::string& name) :
	x0d::XzeroPlugin(d, name),
	directors_(),
	roadWarrior_(),
	haproxyApi_(new HaproxyApi(&directors_))
{
    setupFunction("director.load", &DirectorPlugin::director_load)
        .param<FlowString>("name")
        .param<FlowString>("path");

#if defined(ENABLE_DIRECTOR_CACHE)
    mainFunction("director.cache", &DirectorPlugin::director_cache_enabled, FlowType::Boolean);
    mainFunction("director.cache.key", &DirectorPlugin::director_cache_key, FlowType::String);
    mainFunction("director.cache.ttl", &DirectorPlugin::director_cache_ttl, FlowType::Number);
#endif

    mainHandler("director.balance", &DirectorPlugin::director_balance)
        .param<FlowString>("director")
        .param<FlowString>("bucket", "");

    mainHandler("director.pass", &DirectorPlugin::director_pass)
        .param<FlowString>("director")
        .param<FlowString>("backend");

    mainHandler("director.api", &DirectorPlugin::director_api)
        .param<FlowString>("prefix", "/");

    mainHandler("director.fcgi", &DirectorPlugin::director_fcgi)
        .param<IPAddress>("address", IPAddress("0.0.0.0"))
        .param<int>("port");

    mainHandler("director.http", &DirectorPlugin::director_http)
        .param<IPAddress>("address", IPAddress("0.0.0.0"))
        .param<int>("port");

    mainHandler("director.haproxy_stats", &DirectorPlugin::director_haproxy_stats)
        .param<FlowString>("prefix", "/");

    mainHandler("director.haproxy_monitor", &DirectorPlugin::director_haproxy_monitor)
        .param<FlowString>("prefix", "/");

    roadWarrior_ = new RoadWarrior(server().selectWorker());
}

DirectorPlugin::~DirectorPlugin()
{
	for (auto director: directors_)
		delete director.second;

	delete roadWarrior_;
	delete haproxyApi_;
}

RequestNotes* DirectorPlugin::requestNotes(HttpRequest* r)
{
	if (auto notes = r->customData<RequestNotes>(this))
		return notes;

	return r->setCustomData<RequestNotes>(this, r);
}

// {{{ setup_function director.load(name, path)
void DirectorPlugin::director_load(FlowVM::Params& args)
{
    const auto directorName = args.get<std::string>(1);
    const auto path = args.get<std::string>(2);

    // TODO verify that given director name is still available.
    // TODO verify that no director has been instanciated already by given path.

    server().log(Severity::debug, "director: Loading director %s from %s.", directorName.c_str(), path.c_str());

    Director* director = new Director(server().nextWorker(), directorName);
    if (!director)
        return;

    director->load(path);

    directors_[directorName] = director;
}
// }}}
// {{{ setup function director.cache.key(string key)
#if defined(ENABLE_DIRECTOR_CACHE)
void DirectorPlugin::director_cache_key(HttpRequest* r, FlowVM::Params& args)
{
	auto notes = requestNotes(r);
    notes->setCacheKey(args.get<FlowString>(1));
}
#endif
// }}}
// {{{ function director.cache.enabled()
#if defined(ENABLE_DIRECTOR_CACHE)
void DirectorPlugin::director_cache_enabled(HttpRequest* r, FlowVM::Params& args)
{
    auto notes = requestNotes(r);
    notes->cacheIgnore = !args.get<bool>(1);
}
#endif
// }}}
// {{{ function director.cache.ttl()
#if defined(ENABLE_DIRECTOR_CACHE)
void DirectorPlugin::director_cache_ttl(HttpRequest* r, FlowVM::Params& args)
{
    auto notes = requestNotes(r);
    notes->cacheTTL = TimeSpan::fromSeconds(args.get<FlowNumber>(1));
}
#endif
// }}}
// {{{ handler director.balance(string director_id [, string segment_id ] );
/**
 * \returns always true.
 */
bool DirectorPlugin::internalServerError(HttpRequest* r)
{
	if (r->status != HttpStatus::Undefined)
		r->status = HttpStatus::InternalServerError;

	r->finish();
	return true;
}

// handler director.balance(string director_name, string bucket_name = '');
bool DirectorPlugin::director_balance(HttpRequest* r, FlowVM::Params& args)
{
	std::string directorName = args.get<FlowString>(1).str();
	std::string bucketName = args.get<FlowString>(2).str();

    auto i = directors_.find(directorName);
    if (i == directors_.end()) {
        r->log(Severity::error, "director.balance(): No director with name '%s' configured.", directorName.c_str());
        return internalServerError(r);
    }
	Director* director = i->second;

	RequestShaper::Node* bucket = nullptr;
    if (!bucketName.empty()) {
        bucket = director->findBucket(bucketName);
        if (!bucket) {
            // explicit bucket specified, but not found -> ignore.
            bucket = director->rootBucket();
            r->log(Severity::error, "director: Requested bucket '%s' not found in director '%s'. Assigning root bucket.",
                bucketName.c_str(), directorName.c_str());
        }
    } else {
        bucket = director->rootBucket();
    }

	auto rn = requestNotes(r);
	rn->manager = director;

#if !defined(NDEBUG)
	server().log(Severity::debug, "director: passing request to %s [%s].", director->name().c_str(), bucket->name().c_str());
#endif

	director->schedule(rn, bucket);
	return true;
} // }}}
// {{{ handler director.pass(string director_id [, string backend_id ] );
bool DirectorPlugin::director_pass(HttpRequest* r, FlowVM::Params& args)
{
    std::string directorName = args.get<FlowString>(1).str();
    auto i = directors_.find(directorName);
    if (i == directors_.end()) {
        r->log(Severity::error, "director.pass(): No director with name '%s' configured.", directorName.c_str());
        return internalServerError(r);
    }
    Director* director = i->second;

    // custom backend route
    std::string backendName = args.get<FlowString>(2).str();
    Backend* backend = nullptr;
    if (!backendName.empty()) {
        backend = director->findBackend(backendName);

        if (!backend) {
            // explicit backend specified, but not found -> do not serve.
            r->log(Severity::error, "director: Requested backend '%s' not found.", backendName.c_str());
            return internalServerError(r);
        }
    }

#if !defined(NDEBUG)
	server().log(Severity::debug, "director: passing request to %s [backend %s].", director->name().c_str(), backend->name().c_str());
#endif

	auto rn = requestNotes(r);
	rn->manager = director;

	director->schedule(rn, backend);

	return true;
}
// }}}
// {{{ handler director.api(string prefix);
bool DirectorPlugin::director_api(HttpRequest* r, FlowVM::Params& args)
{
    const FlowString* prefix = args.get<FlowString*>(1);

    if (!r->path.begins(*prefix))
        return false;

    BufferRef path(r->path.ref(prefix->size()));

    return ApiRequest::process(&directors_, r, path);
}
// }}}
// {{{ handler director.fcgi(socketspec);
bool DirectorPlugin::director_fcgi(HttpRequest* r, FlowVM::Params& args)
{
    SocketSpec socketSpec(
        args.get<IPAddress>(1),  // bind addr
        args.get<FlowNumber>(2)  // port
    );

    roadWarrior_->handleRequest(requestNotes(r), socketSpec, RoadWarrior::FCGI);
    return true;
}
// }}}
// {{{ handler director.http(socketspec);
bool DirectorPlugin::director_http(HttpRequest* r, FlowVM::Params& args)
{
    SocketSpec socketSpec(
        args.get<IPAddress>(1),  // bind addr
        args.get<FlowNumber>(2)  // port
    );

    roadWarrior_->handleRequest(requestNotes(r), socketSpec, RoadWarrior::HTTP);
    return true;
}
// }}}
// {{{ haproxy compatibility API
bool DirectorPlugin::director_haproxy_monitor(HttpRequest* r, FlowVM::Params& args)
{
	const FlowString* prefix = args.get<FlowString*>(1);

	if (!r->path.begins(*prefix) && !r->unparsedUri.begins(*prefix))
		return false;

	haproxyApi_->monitor(r);
	return true;
}

bool DirectorPlugin::director_haproxy_stats(HttpRequest* r, FlowVM::Params& args)
{
	const FlowString* prefix = args.get<FlowString*>(1);

	if (!r->path.begins(*prefix) && !r->unparsedUri.begins(*prefix))
		return false;

	haproxyApi_->stats(r, prefix->str());
	return true;
}
// }}}
X0_EXPORT_PLUGIN_CLASS(DirectorPlugin)
