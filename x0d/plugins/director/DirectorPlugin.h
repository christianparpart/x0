// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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

class DirectorPlugin : public x0d::XzeroPlugin {
 private:
  std::unordered_map<std::string, std::unique_ptr<Director>> directors_;
  std::unique_ptr<RoadWarrior> roadWarrior_;
  std::unique_ptr<HaproxyApi> haproxyApi_;
  std::string pseudonym_;

 public:
  DirectorPlugin(x0d::XzeroDaemon* d, const std::string& name);
  ~DirectorPlugin();

 private:
  RequestNotes* requestNotes(x0::HttpRequest* r);

  void director_load(x0::FlowVM::Params& args);

  void director_cache_enabled(x0::HttpRequest* r, x0::FlowVM::Params& args);
  void director_cache_key(x0::HttpRequest* r, x0::FlowVM::Params& args);
  void director_cache_ttl(x0::HttpRequest* r, x0::FlowVM::Params& args);

  void director_pseudonym(x0::FlowVM::Params& args);

  bool director_balance(x0::HttpRequest* r, x0::FlowVM::Params& args);
  void balance(x0::HttpRequest* r, const std::string& directorName,
               const std::string& bucketName);

  bool director_pass(x0::HttpRequest* r, x0::FlowVM::Params& args);
  void pass(x0::HttpRequest* r, const std::string& directorName,
            const std::string& backendName);

  bool director_api(x0::HttpRequest* r, x0::FlowVM::Params& args);
  bool director_fcgi(x0::HttpRequest* r, x0::FlowVM::Params& args);
  bool director_http(x0::HttpRequest* r, x0::FlowVM::Params& args);

  bool director_haproxy_monitor(x0::HttpRequest* r, x0::FlowVM::Params& args);
  bool director_haproxy_stats(x0::HttpRequest* r, x0::FlowVM::Params& args);

  bool director_roadwarrior_verify(x0::Instr* instr);

  bool internalServerError(x0::HttpRequest* r);

  void addVia(x0::HttpRequest* r);
};
