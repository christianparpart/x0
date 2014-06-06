/* <plugins/director/DirectorPlugin.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0d/XzeroPlugin.h>
#include <x0/http/HttpServer.h>

#include <string>
#include <memory>
#include <unordered_map>

namespace x0 {
    class HttpServer;
    class HttpRequest;
}

class Director;
class RoadWarrior;
class Backend;
class HaproxyApi;
struct RequestNotes;

class DirectorPlugin :
    public x0d::XzeroPlugin
{
private:
    std::unordered_map<std::string, Director*> directors_;
    std::unique_ptr<RoadWarrior> roadWarrior_;
    std::unique_ptr<HaproxyApi> haproxyApi_;
    x0::HttpServer::RequestHook::Connection postProcess_;

public:
    DirectorPlugin(x0d::XzeroDaemon* d, const std::string& name);
    ~DirectorPlugin();

private:
    RequestNotes* requestNotes(x0::HttpRequest* r);

    void director_load(x0::FlowVM::Params& args);

    void director_cache_enabled(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void director_cache_key(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void director_cache_ttl(x0::HttpRequest* r, x0::FlowVM::Params& args);

    bool director_balance(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void balance(x0::HttpRequest* r, const std::string& directorName, const std::string& bucketName, std::unique_ptr<x0::Source>&& body);

    bool director_pass(x0::HttpRequest* r, x0::FlowVM::Params& args);
    void pass(x0::HttpRequest* r, const std::string& directorName, const std::string& backendName, std::unique_ptr<x0::Source>&& body);

    bool director_api(x0::HttpRequest* r, x0::FlowVM::Params& args);
    bool director_fcgi(x0::HttpRequest* r, x0::FlowVM::Params& args);
    bool director_http(x0::HttpRequest* r, x0::FlowVM::Params& args);

    bool director_haproxy_monitor(x0::HttpRequest* r, x0::FlowVM::Params& args);
    bool director_haproxy_stats(x0::HttpRequest* r, x0::FlowVM::Params& args);

    bool director_roadwarrior_verify(x0::Instr* instr);

    bool internalServerError(x0::HttpRequest* r);
};
