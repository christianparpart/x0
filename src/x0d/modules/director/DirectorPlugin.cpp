// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/*
 * plugin type: content generator
 *
 * description:
 *     Implements request proxying and load balancing.
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
#include "ClientAbortAction.h"

#include <xzero/HttpServer.h>
#include <xzero/HttpRequest.h>
#include <flow/ir/ConstantValue.h>
#include <flow/ir/Instr.h>
#include <base/SocketSpec.h>
#include <base/Url.h>
#include <base/Types.h>

using namespace xzero;
using namespace flow;
using namespace base;

DirectorPlugin::DirectorPlugin(x0d::XzeroDaemon* d, const std::string& name)
    : x0d::XzeroPlugin(d, name),
      directors_(),
      roadWarrior_(new RoadWarrior(server().selectWorker())),
      haproxyApi_(new HaproxyApi(&directors_)),
      pseudonym_("x0d") {

  setupFunction("director.load", &DirectorPlugin::director_load)
      .param<FlowString>("name")
      .param<FlowString>("path");

  setupFunction("director.pseudonym", &DirectorPlugin::director_pseudonym,
                FlowType::String);

#if defined(ENABLE_DIRECTOR_CACHE)
  mainFunction("director.cache", &DirectorPlugin::director_cache_enabled,
               FlowType::Boolean);
  mainFunction("director.cache.key", &DirectorPlugin::director_cache_key,
               FlowType::String);
  mainFunction("director.cache.ttl", &DirectorPlugin::director_cache_ttl,
               FlowType::Number);
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
      .verifier(&DirectorPlugin::director_roadwarrior_verify, this)
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<FlowString>("on_client_abort", "close");

  mainHandler("director.http", &DirectorPlugin::director_http)
      .verifier(&DirectorPlugin::director_roadwarrior_verify, this)
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<FlowString>("on_client_abort", "close");

  mainHandler("director.haproxy_stats", &DirectorPlugin::director_haproxy_stats)
      .param<FlowString>("prefix", "/");

  mainHandler("director.haproxy_monitor",
              &DirectorPlugin::director_haproxy_monitor)
      .param<FlowString>("prefix", "/");
}

DirectorPlugin::~DirectorPlugin() {}

RequestNotes* DirectorPlugin::requestNotes(HttpRequest* r) {
  if (auto notes = r->customData<RequestNotes>(this)) return notes;

  return r->setCustomData<RequestNotes>(this, r);
}

void DirectorPlugin::addVia(HttpRequest* r) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%d.%d %s", r->httpVersionMajor,
           r->httpVersionMinor, pseudonym_.c_str());

  // RFC 7230, section 5.7.1: makes it clear, that we put ourselfs into the
  // front of the Via-list.
  r->responseHeaders.prepend("Via", buf);
}

void DirectorPlugin::director_pseudonym(flow::vm::Params& args) {
  pseudonym_ = args.getString(1).str();
}

// {{{ setup_function director.load(name, path)
void DirectorPlugin::director_load(vm::Params& args) {
  const auto directorName = args.getString(1).str();
  const auto path = args.getString(2).str();

  // TODO verify that given director name is still available.
  // TODO verify that no director has been instanciated already by given path.

  server().log(Severity::trace, "director: Loading director %s from %s.",
               directorName.c_str(), path.c_str());

  std::unique_ptr<Director> director(
      new Director(server().nextWorker(), directorName));
  if (!director) return;

  director->load(path);

  directors_[directorName] = std::move(director);
}
// }}}
// {{{ setup function director.cache.key(string key)
#if defined(ENABLE_DIRECTOR_CACHE)
void DirectorPlugin::director_cache_key(HttpRequest* r, vm::Params& args) {
  auto notes = requestNotes(r);
  notes->setCacheKey(args.getString(1));
}
#endif
// }}}
// {{{ function director.cache.enabled()
#if defined(ENABLE_DIRECTOR_CACHE)
void DirectorPlugin::director_cache_enabled(HttpRequest* r,
                                            vm::Params& args) {
  auto notes = requestNotes(r);
  notes->cacheIgnore = !args.getBool(1);
}
#endif
// }}}
// {{{ function director.cache.ttl()
#if defined(ENABLE_DIRECTOR_CACHE)
void DirectorPlugin::director_cache_ttl(HttpRequest* r, vm::Params& args) {
  auto notes = requestNotes(r);
  notes->cacheTTL = Duration::fromSeconds(args.getInt(1));
}
#endif
// }}}
// {{{ handler director.balance(string director_id [, string segment_id ] );
/**
 * \returns always true.
 */
bool DirectorPlugin::internalServerError(HttpRequest* r) {
  if (r->status != HttpStatus::Undefined)
    r->status = HttpStatus::InternalServerError;

  r->finish();
  return true;
}

// handler director.balance(string director_name, string bucket_name = '');
bool DirectorPlugin::director_balance(HttpRequest* r, vm::Params& args) {
  std::string directorName = args.getString(1).str();
  std::string bucketName = args.getString(2).str();

  balance(r, directorName, bucketName);

  return true;
}

void DirectorPlugin::balance(HttpRequest* r, const std::string& directorName,
                             const std::string& bucketName) {
  auto i = directors_.find(directorName);
  if (i == directors_.end()) {
    r->log(Severity::error,
           "director.balance(): No director with name '%s' configured.",
           directorName.c_str());
    internalServerError(r);
    return;
  }
  Director* director = i->second.get();

  RequestShaper::Node* bucket = nullptr;
  if (!bucketName.empty()) {
    bucket = director->findBucket(bucketName);
    if (!bucket) {
      // explicit bucket specified, but not found -> ignore.
      bucket = director->rootBucket();
      r->log(Severity::error,
             "director: Requested bucket '%s' not found in director '%s'. "
             "Assigning root bucket.",
             bucketName.c_str(), directorName.c_str());
    }
  } else {
    bucket = director->rootBucket();
  }

  auto rn = requestNotes(r);
  rn->manager = director;

  r->onPostProcess.connect(std::bind(&DirectorPlugin::addVia, this, r));

#if !defined(NDEBUG)
  server().log(Severity::trace, "director: passing request to %s [%s].",
               director->name().c_str(), bucket->name().c_str());
#endif

  director->schedule(rn, bucket);
}  // }}}
// {{{ handler director.pass(string director_id [, string backend_id ] );
bool DirectorPlugin::director_pass(HttpRequest* r, vm::Params& args) {
  std::string directorName = args.getString(1).str();
  std::string backendName = args.getString(2).str();

  pass(r, directorName, backendName);
  return true;
}

void DirectorPlugin::pass(HttpRequest* r, const std::string& directorName,
                          const std::string& backendName) {
  auto i = directors_.find(directorName);
  if (i == directors_.end()) {
    r->log(Severity::error,
           "director.pass(): No director with name '%s' configured.",
           directorName.c_str());
    internalServerError(r);
    return;
  }
  Director* director = i->second.get();

  // custom backend route
  Backend* backend = nullptr;
  if (!backendName.empty()) {
    backend = director->findBackend(backendName);

    if (!backend) {
      // explicit backend specified, but not found -> do not serve.
      r->log(Severity::error, "director: Requested backend '%s' not found.",
             backendName.c_str());
      internalServerError(r);
      return;
    }
  }

#if !defined(NDEBUG)
  server().log(Severity::trace, "director: passing request to %s [backend %s].",
               director->name().c_str(), backend->name().c_str());
#endif

  auto rn = requestNotes(r);
  rn->manager = director;

  r->onPostProcess.connect(std::bind(&DirectorPlugin::addVia, this, r));

  director->schedule(rn, backend);

  return;
}
// }}}
// {{{ handler director.api(string prefix);
bool DirectorPlugin::director_api(HttpRequest* r, vm::Params& args) {
  const FlowString& prefix = args.getString(1);

  if (!r->path.begins(prefix)) return false;

  BufferRef path(r->path.ref(prefix.size()));

  if (path.empty()) path = "/";

  return ApiRequest::process(&directors_, r, path);
}
// }}}
// {{{ handler director.fcgi(string hostname, int port, string onClientAbort =
// "close");
bool DirectorPlugin::director_fcgi(HttpRequest* r, vm::Params& args) {
  RequestNotes* rn = requestNotes(r);

  SocketSpec socketSpec(args.getIPAddress(1),  // bind addr
                        args.getInt(2)         // port
                        );

  Try<ClientAbortAction> value = parseClientAbortAction(args.getString(3));
  if (value.isError()) {
    goto done;
  }

  rn->onClientAbort = value.get();

  r->onPostProcess.connect(std::bind(&DirectorPlugin::addVia, this, r));

  roadWarrior_->handleRequest(rn, socketSpec, RoadWarrior::FCGI);

done:
  return true;
}
// }}}
// {{{ handler director.http(address, port, on_client_abort);
bool DirectorPlugin::director_roadwarrior_verify(Instr* instr) {
  if (auto s = dynamic_cast<ConstantString*>(instr->operand(3))) {
    Try<ClientAbortAction> op = parseClientAbortAction(s->get());
    if (op) {
      // okay: XXX we could hard-replace the 3rd arg here, as we pre-parsed it,
      // kinda
      return true;
    } else {
      log(Severity::error,
          "on_client_abort argument must a literal value of value 'close', "
          "'notify', 'ignore'.");
      return false;
    }
  } else {
    log(Severity::error, "on_client_abort argument must be a literal.");
    return false;
  }
}

bool DirectorPlugin::director_http(HttpRequest* r, vm::Params& args) {
  SocketSpec socketSpec(args.getIPAddress(1),  // bind addr
                        args.getInt(2)         // port
                        );

  r->onPostProcess.connect(std::bind(&DirectorPlugin::addVia, this, r));

  roadWarrior_->handleRequest(requestNotes(r), socketSpec, RoadWarrior::HTTP);

  return true;
}
// }}}
// {{{ haproxy compatibility API
bool DirectorPlugin::director_haproxy_monitor(HttpRequest* r,
                                              vm::Params& args) {
  const FlowString& prefix = args.getString(1);

  if (!r->path.begins(prefix) && !r->unparsedUri.begins(prefix)) return false;

  haproxyApi_->monitor(r);
  return true;
}

bool DirectorPlugin::director_haproxy_stats(HttpRequest* r,
                                            vm::Params& args) {
  const FlowString& prefix = args.getString(1);

  if (!r->path.begins(prefix) && !r->unparsedUri.begins(prefix)) return false;

  haproxyApi_->stats(r, prefix.str());
  return true;
}
// }}}
X0D_EXPORT_PLUGIN_CLASS(DirectorPlugin)
