// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "XzeroModule.h"
#include "XzeroContext.h"
#include "modules/core.h"
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/io/FileUtil.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/Tokenizer.h>
#include <sstream>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

namespace x0d {

ProxyModule::ProxyModule(XzeroDaemon* d)
    : XzeroModule(d, "proxy"),
      pseudonym_("x0d"),
      clusters_() {

  mainHandler("proxy.cluster", &ProxyModule::proxy_cluster)
      .param<FlowString>("config")
      .param<FlowString>("bucket", "");
      .param<FlowString>("backend", "");
      .verifier(&ProxyModule::verify_proxy_cluster, this);

  mainHandler("proxy.api", &ProxyPlugin::director_api)
      .param<FlowString>("prefix", "/");

  mainHandler("proxy.fcgi", &ProxyPlugin::director_fcgi)
      .verifier(&ProxyPlugin::director_roadwarrior_verify, this)
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<FlowString>("on_client_abort", "close");

  mainHandler("proxy.http", &ProxyPlugin::director_http)
      .verifier(&ProxyPlugin::director_roadwarrior_verify, this)
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<FlowString>("on_client_abort", "close");

  mainHandler("proxy.haproxy_stats", &ProxyPlugin::director_haproxy_stats)
      .param<FlowString>("prefix", "/");

  mainHandler("proxy.haproxy_monitor", &ProxyPlugin::director_haproxy_monitor)
      .param<FlowString>("prefix", "/");

  setupFunction("director.pseudonym", &ProxyPlugin::director_pseudonym,
                FlowType::String);

#if defined(ENABLE_DIRECTOR_CACHE)
  mainFunction("director.cache", &ProxyPlugin::director_cache_enabled,
               FlowType::Boolean);
  mainFunction("director.cache.key", &ProxyPlugin::director_cache_key,
               FlowType::String);
  mainFunction("director.cache.ttl", &ProxyPlugin::director_cache_ttl,
               FlowType::Number);
#endif

}

ProxyModule::~ProxyModule() {
}

bool ProxyModule::verify_proxy_cluster(xzero::flow::Instr* call) {
  return false;
}

bool ProxyModule::proxy_cluster(XzeroContext* cx, Params& args) {
  return false;
}

} // namespace x0d
