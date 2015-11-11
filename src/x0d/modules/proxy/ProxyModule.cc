// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "ProxyModule.h"
#include "XzeroContext.h"
#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/io/FileUtil.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/Tokenizer.h>
#include <xzero-flow/AST.h>
#include <xzero-flow/ir/Instr.h>
#include <xzero-flow/ir/BasicBlock.h>
#include <xzero-flow/ir/IRHandler.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/ConstantValue.h>
#include <xzero-flow/ir/ConstantArray.h>
#include <sstream>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

namespace x0d {

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

ProxyModule::ProxyModule(XzeroDaemon* d)
    : XzeroModule(d, "proxy"),
      pseudonym_("x0d"),
      clusters_() {

  setupFunction("proxy.pseudonym", &ProxyModule::proxy_pseudonym,
                FlowType::String);

  mainHandler("proxy.cluster", &ProxyModule::proxy_cluster)
      .param<FlowString>("name")
      .param<FlowString>("path", "")
      .param<FlowString>("bucket", "")
      .param<FlowString>("backend", "")
      .verifier(&ProxyModule::verify_proxy_cluster, this);

  mainHandler("proxy.api", &ProxyModule::proxy_api)
      .param<FlowString>("prefix", "/");

  mainHandler("proxy.fcgi", &ProxyModule::proxy_fcgi)
      .verifier(&ProxyModule::proxy_roadwarrior_verify, this)
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<FlowString>("on_client_abort", "close");

  mainHandler("proxy.http", &ProxyModule::proxy_http)
      .verifier(&ProxyModule::proxy_roadwarrior_verify, this)
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<FlowString>("on_client_abort", "close");

  mainHandler("proxy.haproxy_stats", &ProxyModule::proxy_haproxy_stats)
      .param<FlowString>("prefix", "/");

  mainHandler("proxy.haproxy_monitor", &ProxyModule::proxy_haproxy_monitor)
      .param<FlowString>("prefix", "/");

  mainFunction("proxy.cache", &ProxyModule::proxy_cache_enabled,
               FlowType::Boolean);
  mainFunction("proxy.cache.key", &ProxyModule::proxy_cache_key,
               FlowType::String);
  mainFunction("proxy.cache.ttl", &ProxyModule::proxy_cache_ttl,
               FlowType::Number);

  clusters_.emplace_back(new client::HttpCluster());
}

ProxyModule::~ProxyModule() {
}

void ProxyModule::proxy_pseudonym(xzero::flow::vm::Params& args) {
  pseudonym_ = args.getString(1).str();
}

bool ProxyModule::verify_proxy_cluster(xzero::flow::Instr* call) {
  return false; // TODO
}

bool ProxyModule::proxy_cluster(XzeroContext* cx, Params& args) {
  /*
   * @param name
   * @param path
   * @param bucket
   * @param backend
   */

  return false; // TODO
}

bool ProxyModule::proxy_pass(XzeroContext* cx, xzero::flow::vm::Params& args) {
  return false; // TODO
}

bool ProxyModule::proxy_api(XzeroContext* cx, xzero::flow::vm::Params& args) {
  return false; // TODO
}

bool ProxyModule::proxy_fcgi(XzeroContext* cx, xzero::flow::vm::Params& args) {
  return false; // TODO
}

bool ProxyModule::proxy_http(XzeroContext* cx, xzero::flow::vm::Params& args) {
  IPAddress ipaddr = args.getIPAddress(1);
  FlowNumber port = args.getInt(2);
  FlowString onClientAbortStr = args.getString(3);
  Duration connectTimeout = Duration::fromSeconds(16);
  Executor* executor = cx->response()->executor();

  InetEndPoint::connectAsync(
      ipaddr,
      port,
      connectTimeout,
      executor,
      std::bind(&ProxyModule::proxyHttpConnected, this,
                std::placeholders::_1, cx),
      std::bind(&ProxyModule::proxyHttpConnectFailed, this,
                std::placeholders::_1, ipaddr, port, cx));

  return true;
}

void ProxyModule::proxyHttpConnected(RefPtr<InetEndPoint> ep, XzeroContext* cx) {
  Executor* executor = cx->response()->executor();
  HttpRequest* request = cx->request();

  // TODO: make_shared<client::HttpClient>(scheduler, ep.as<EndPoint>())
  client::HttpClient* cli = new client::HttpClient(executor, ep.as<EndPoint>());

  std::string requestBody = ""; // TODO
  size_t requestBodySize = requestBody.size();

  HttpRequestInfo requestInfo(
      HttpVersion::VERSION_1_1,
      request->unparsedMethod(),
      request->unparsedUri(),
      requestBodySize,
      request->headers());

  cli->send(std::move(requestInfo), requestBody);

  Future<client::HttpClient*> f = cli->completed();

  f.onSuccess(std::bind(&ProxyModule::proxyHttpRespond, this, cli, cx));
  f.onFailure(std::bind(&ProxyModule::proxyHttpRespondFailure, this,
                        std::placeholders::_1, cli, cx));
}

static bool isConnectionHeader(const std::string& name) {
  static const std::vector<std::string> connectionHeaderFields = {
    "Connection",
    "Content-Length",
    "Close",
    "Keep-Alive",
    "TE",
    "Trailer",
    "Transfer-Encoding",
    "Upgrade",
    "Via",
  };

  for (const auto& test: connectionHeaderFields)
    if (iequals(name, test))
      return true;

  return false;
}

void ProxyModule::proxyHttpRespond(client::HttpClient* cli,
                                   XzeroContext* cx) {
  for (const HeaderField& field: cli->responseInfo().headers()) {
    if (!isConnectionHeader(field.name())) {
      cx->response()->headers().push_back(field.name(), field.value());
    }
  }
  addVia(cx);
  cx->response()->setStatus(cli->responseInfo().status());
  cx->response()->setReason(cli->responseInfo().reason());
  cx->response()->setContentLength(cli->responseBody().size());
  cx->response()->output()->write(cli->responseBody());
  cx->response()->completed();
}

void ProxyModule::addVia(XzeroContext* cx) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%s %s",
           StringUtil::toString(cx->request()->version()).c_str(),
           pseudonym_.c_str());

  // RFC 7230, section 5.7.1: makes it clear, that we put ourselfs into the
  // front of the Via-list.
  cx->response()->headers().prepend("Via", buf);
}

void ProxyModule::proxyHttpRespondFailure(const Status& status,
                                          client::HttpClient* cli,
                                          XzeroContext* cx) {
  logError("proxy", "Failure detected while receiving upstream response. $0", status);
  cx->response()->setStatus(HttpStatus::BadGateway);
  cx->response()->completed();
}

void ProxyModule::proxyHttpConnectFailed(
    Status error,
    const IPAddress& ipaddr,
    int port,
    XzeroContext* cx) {
  logError("proxy",
           "Coould not connect to $0:$1. $2",
           ipaddr, port, error);

  cx->response()->setStatus(HttpStatus::ServiceUnavailable);
  cx->response()->completed();
}

bool ProxyModule::proxy_haproxy_monitor(XzeroContext* cx, xzero::flow::vm::Params& args) {
  return false; // TODO
}

bool ProxyModule::proxy_haproxy_stats(XzeroContext* cx, xzero::flow::vm::Params& args) {
  return false; // TODO
}

bool ProxyModule::proxy_roadwarrior_verify(xzero::flow::Instr* instr) {
  return true; // TODO
}

void ProxyModule::proxy_cache_enabled(XzeroContext* cx, xzero::flow::vm::Params& args) {
}

void ProxyModule::proxy_cache_key(XzeroContext* cx, xzero::flow::vm::Params& args) {
}

void ProxyModule::proxy_cache_ttl(XzeroContext* cx, xzero::flow::vm::Params& args) {
}

} // namespace x0d
