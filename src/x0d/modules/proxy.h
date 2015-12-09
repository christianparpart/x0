// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include "XzeroModule.h"
#include <xzero/logging.h>
#include <xzero/http/client/HttpClusterApi.h>
#include <xzero-flow/AST.h>
#include <xzero-flow/ir/Instr.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <list>

namespace xzero {
  namespace http {
    class HttpRequestInfo;
    class HttpResponseInfo;
    namespace client {
      class HttpCluster;
      class HttpClient;
    }
  }
}

namespace x0d {

class ProxyModule : public XzeroModule,
                    public xzero::http::client::HttpClusterApi {
 public:
  explicit ProxyModule(XzeroDaemon* d);
  ~ProxyModule();

  const std::string& pseudonym() const noexcept { return pseudonym_; }

  // helpers
  void addVia(XzeroContext* cx);
  void addVia(const xzero::http::HttpRequestInfo* in,
              xzero::http::HttpResponse* out);

  // HttpClusterApi overrides
  std::list<xzero::http::client::HttpCluster*> listCluster() override;
  xzero::http::client::HttpCluster* findCluster(const std::string& name) override;
  xzero::http::client::HttpCluster* createCluster(const std::string& name, const std::string& path) override;
  void destroyCluster(const std::string& name) override;

 private:
  // setup functions
  void proxy_pseudonym(xzero::flow::vm::Params& args);

  // main handlers
  xzero::http::client::HttpCluster* findLocalCluster(const std::string& host);
  bool proxy_cluster_auto(XzeroContext* cx, xzero::flow::vm::Params& args);
  bool verify_proxy_cluster(xzero::flow::Instr* call);
  bool proxy_cluster(XzeroContext* cx, Params& args);
  bool proxy_pass(XzeroContext* cx, xzero::flow::vm::Params& args);
  bool proxy_api(XzeroContext* cx, xzero::flow::vm::Params& args);
  bool proxy_fcgi(XzeroContext* cx, xzero::flow::vm::Params& args);
  bool proxy_http(XzeroContext* cx, xzero::flow::vm::Params& args);
  bool proxy_haproxy_monitor(XzeroContext* cx, xzero::flow::vm::Params& args);
  bool proxy_haproxy_stats(XzeroContext* cx, xzero::flow::vm::Params& args);
  bool proxy_roadwarrior_verify(xzero::flow::Instr* instr);
  void proxy_cache_enabled(XzeroContext* cx, xzero::flow::vm::Params& args);
  void proxy_cache_key(XzeroContext* cx, xzero::flow::vm::Params& args);
  void proxy_cache_ttl(XzeroContext* cx, xzero::flow::vm::Params& args);
  bool tryHandleTrace(XzeroContext* cx);

 private:
  bool internalServerError(XzeroContext* cx);

  void balance(XzeroContext* cx,
               const std::string& directorName,
               const std::string& bucketName);

  void pass(XzeroContext* cx,
            const std::string& directorName,
            const std::string& backendName);

  void proxyHttpConnected(xzero::RefPtr<xzero::EndPoint> ep,
                          XzeroContext* cx);
  void proxyHttpConnectFailed(xzero::Status error,
                              const xzero::IPAddress& ipaddr, int port,
                              XzeroContext* cx);
  void proxyHttpRespond(XzeroContext* cx);
  void proxyHttpRespondFailure(const xzero::Status& status,
                               XzeroContext* cx);

  void onPostConfig() override;

 private:
  std::string pseudonym_;

  // list of clusters by {name, path} to initialize
  std::unordered_map<std::string, std::string> clusterInit_;

  std::unordered_map<
      std::string,
      std::shared_ptr<xzero::http::client::HttpCluster>>
          clusterMap_;
};

} // namespace x0d
