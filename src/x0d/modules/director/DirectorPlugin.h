// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/XzeroPlugin.h>

#include <string>
#include <memory>
#include <unordered_map>

namespace xzero {
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
  RequestNotes* requestNotes(xzero::HttpRequest* r);

  void director_load(flow::vm::Params& args);

  void director_cache_enabled(xzero::HttpRequest* r, flow::vm::Params& args);
  void director_cache_key(xzero::HttpRequest* r, flow::vm::Params& args);
  void director_cache_ttl(xzero::HttpRequest* r, flow::vm::Params& args);

  void director_pseudonym(flow::vm::Params& args);

  bool director_balance(xzero::HttpRequest* r, flow::vm::Params& args);
  void balance(xzero::HttpRequest* r, const std::string& directorName,
               const std::string& bucketName);

  bool director_pass(xzero::HttpRequest* r, flow::vm::Params& args);
  void pass(xzero::HttpRequest* r, const std::string& directorName,
            const std::string& backendName);

  bool director_api(xzero::HttpRequest* r, flow::vm::Params& args);
  bool director_fcgi(xzero::HttpRequest* r, flow::vm::Params& args);
  bool director_http(xzero::HttpRequest* r, flow::vm::Params& args);

  bool director_haproxy_monitor(xzero::HttpRequest* r, flow::vm::Params& args);
  bool director_haproxy_stats(xzero::HttpRequest* r, flow::vm::Params& args);

  bool director_roadwarrior_verify(flow::Instr* instr);

  bool internalServerError(xzero::HttpRequest* r);

  void addVia(xzero::HttpRequest* r);
};
