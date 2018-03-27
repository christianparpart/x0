// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/Module.h>
#include <xzero/logging.h>
#include <xzero/http/cluster/Api.h>
#include <xzero-flow/AST.h>
#include <xzero-flow/ir/Instr.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <list>

namespace xzero {
  namespace flow {
    class IRBuilder;
  }
  namespace http {
    class HttpRequestInfo;
    namespace cluster {
      class Cluster;
    }
  }
}

namespace x0d {

class ProxyModule : public Module,
                    public xzero::http::cluster::Api {
 public:
  explicit ProxyModule(Daemon* d);
  ~ProxyModule();

  const std::string& pseudonym() const noexcept { return pseudonym_; }

  // helpers
  void addVia(Context* cx);
  void addVia(const xzero::http::HttpRequestInfo* in,
              xzero::http::HttpResponse* out);

  // HttpClusterApi overrides
  std::list<xzero::http::cluster::Cluster*> listCluster() override;
  xzero::http::cluster::Cluster* findCluster(const std::string& name) override;
  xzero::http::cluster::Cluster* createCluster(const std::string& name, const std::string& path) override;
  void destroyCluster(const std::string& name) override;

 private:
  // setup functions
  void proxy_pseudonym(xzero::flow::Params& args);

  // main handlers
  xzero::http::cluster::Cluster* findLocalCluster(const std::string& host);
  bool proxy_cluster_auto(Context* cx, xzero::flow::Params& args);
  bool verify_proxy_cluster(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder);
  bool proxy_cluster(Context* cx, Params& args);
  bool proxy_api(Context* cx, xzero::flow::Params& args);
  bool proxy_fcgi(Context* cx, xzero::flow::Params& args);
  bool proxy_http(Context* cx, xzero::flow::Params& args);
  bool proxy_roadwarrior_verify(xzero::flow::Instr* instr, xzero::flow::IRBuilder* builder);
  void proxy_cache(Context* cx, xzero::flow::Params& args);

 private:
  bool internalServerError(Context* cx);

  void balance(Context* cx,
               const std::string& clusterName,
               const std::string& bucketName);

  void pass(Context* cx,
            const std::string& clusterName,
            const std::string& backendName);

  void proxyHttpConnected(std::shared_ptr<xzero::TcpEndPoint> ep,
                          Context* cx);
  void proxyHttpConnectFailed(const std::string& errorMessage,
                              const xzero::IPAddress& ipaddr, int port,
                              Context* cx);
  void proxyHttpRespond(Context* cx);
  void proxyHttpRespondFailure(const std::string& errorMessage,
                               Context* cx);

  void onPostConfig() override;

 private:
  std::string pseudonym_;

  // list of clusters by {name, path} to initialize
  std::unordered_map<std::string, std::string> clusterInit_;

  std::unordered_map<std::string,
                     std::shared_ptr<xzero::http::cluster::Cluster>>
                       clusterMap_;
};

} // namespace x0d
