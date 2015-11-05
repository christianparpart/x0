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
#include <xzero-flow/AST.h>
#include <xzero-flow/ir/Instr.h>

namespace xzero {
  namespace http {
    namespace client {
      class HttpCluster;
    }
  }
}

namespace x0d {

class ProxyModule : public XzeroModule {
 public:
  explicit ProxyModule(XzeroDaemon* d);
  ~ProxyModule();

 private:
  // setup functions
  void proxy_pseudonym(xzero::flow::vm::Params& args);

  // main handlers
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

 private:
  bool internalServerError(XzeroContext* cx);

  void addVia(XzeroContext* cx);

  void balance(XzeroContext* cx,
               const std::string& directorName,
               const std::string& bucketName);

  void pass(XzeroContext* cx,
            const std::string& directorName,
            const std::string& backendName);

 private:
  std::string pseudonym_;
  std::list<std::unique_ptr<xzero::http::client::HttpCluster>> clusters_;
};

} // namespace x0d
