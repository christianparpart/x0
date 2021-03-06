// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/*
 * Noteworthy
 *
 * - does not forward Expect request header to upstream (does nginx/apache?)
 */

#include "proxy.h"
#include <x0d/Context.h>
#include <xzero/sysconfig.h>
#include <xzero/http/cluster/Cluster.h>
#include <xzero/http/cluster/Context.h>
#include <xzero/http/cluster/ApiHandler.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/io/FileUtil.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/Application.h>
#include <flow/AST.h>
#include <flow/ir/Instr.h>
#include <flow/ir/BasicBlock.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/IRProgram.h>
#include <flow/ir/ConstantValue.h>
#include <flow/ir/ConstantArray.h>
#include <fmt/format.h>

namespace x0d {

using namespace xzero;
using namespace xzero::http;
using namespace flow;

using xzero::http::cluster::ApiHandler;
using ClusterContext = xzero::http::cluster::Context;
using xzero::http::cluster::Cluster;
using xzero::http::client::HttpClient;

template<typename T>
static bool isConnectionHeader(const T& name) {
  static const std::vector<T> connectionHeaderFields = {
    "Connection",
    "Content-Length",
    "Close",
    "Keep-Alive",
    "TE",
    "Trailer",
    "Transfer-Encoding",
    "Upgrade",
  };

  for (const auto& test: connectionHeaderFields)
    if (iequals(name, test))
      return true;

  return false;
}

ProxyModule::ProxyModule(Daemon* d)
    : Module(d, "proxy"),
      pseudonym_("x0d"),
      clusterInit_(),
      clusterMap_() {

  setupFunction("proxy.pseudonym", &ProxyModule::proxy_pseudonym,
                LiteralType::String);

  mainHandler("proxy.cluster", &ProxyModule::proxy_cluster_auto)
      .setExperimental(); // TODO

  mainHandler("proxy.cluster", &ProxyModule::proxy_cluster)
      .setExperimental() // TODO
      .param<FlowString>("name")
      .param<FlowString>("path", "")
      .param<FlowString>("bucket", "")
      .param<FlowString>("backend", "")
      .verifier(&ProxyModule::verify_proxy_cluster, this);

  mainHandler("proxy.api", &ProxyModule::proxy_api)
      .setExperimental() // TODO
      .param<FlowString>("prefix", "/");

  mainHandler("proxy.fcgi", &ProxyModule::proxy_fcgi)
      .setExperimental() // TODO
      .verifier(&ProxyModule::proxy_roadwarrior_verify, this)
      .param<FlowString>("on_client_abort", "close")
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port");

  mainHandler("proxy.http", &ProxyModule::proxy_http)
      .verifier(&ProxyModule::proxy_roadwarrior_verify, this)
      .param<FlowString>("on_client_abort", "close")
      .param<IPAddress>("address")
      .param<int>("port")
      .param<int>("connect_timeout", 10)
      .param<int>("read_timeout", 60)
      .param<int>("write_timeout", 10);

  mainFunction("proxy.cache", &ProxyModule::proxy_cache)
      .setExperimental() // TODO
      .param<bool>("enabled", true)
      .param<FlowString>("key", "")
      .param<int>("ttl", 0);
}

ProxyModule::~ProxyModule() {
}

void ProxyModule::proxy_pseudonym(flow::Params& args) {
  std::string value = args.getString(1);

  for (char ch: value) {
    if (!std::isalnum(ch) && ch != '_' && ch != '-' && ch != '.') {
      throw ConfigurationError{"Invalid character found in proxy.pseudonym"};
    }
  }

  pseudonym_ = value;
}

bool ProxyModule::verify_proxy_cluster(flow::Instr* call, flow::IRBuilder* builder) {
  auto program = call->getBasicBlock()->getHandler()->getProgram();

  auto nameArg = dynamic_cast<ConstantString*>(call->operand(1));
  if (nameArg == nullptr) {
    logError("proxy.cluster: name parameter must be a literal.");
    return false;
  }

  if (nameArg->get().empty()) {
    logError("Setting empty proxy.cluster name is not allowed.");
    return false;
  }

  // pathArg
  auto pathArg = dynamic_cast<ConstantString*>(call->operand(2));
  if (pathArg == nullptr) {
    logError("proxy.cluster: path parameter must be a literal.");
    return false;
  }

  std::string path = pathArg->get().empty()
      ? FileUtil::joinPaths(XZERO_CLUSTERDIR, nameArg->get() + ".cluster.conf")
      : pathArg->get();

  call->setOperand(2, program->get(path));

  clusterInit_[nameArg->get()] = path;

  return true;
}

void ProxyModule::onPostConfig() {
  for (const auto& init: clusterInit_) {
    std::string name = init.first;
    std::string path = init.second;

    createCluster(name, path);
  }
  clusterInit_.clear();
}

class HttpResponseBuilder : public HttpListener { // {{{
 public:
  explicit HttpResponseBuilder(HttpResponse* response);

  void onMessageBegin(HttpVersion version, HttpStatus code, const BufferRef& text) override;
  void onMessageHeader(const BufferRef& name, const BufferRef& value) override;
  void onMessageHeaderEnd() override;
  void onMessageContent(const BufferRef& chunk) override;
  void onMessageContent(FileView&& chunk) override;
  void onMessageEnd() override;
  void onError(std::error_code ec) override;

 private:
  HttpResponse* response_;
};

HttpResponseBuilder::HttpResponseBuilder(HttpResponse* response)
    : response_(response) {
}

void HttpResponseBuilder::onMessageBegin(HttpVersion version, HttpStatus code, const BufferRef& text) {
  response_->setStatus(code);
  response_->setReason(text.str());
}

void HttpResponseBuilder::onMessageHeader(const BufferRef& name, const BufferRef& value) {
  if (iequals(name, "Content-Length")) {
    response_->setContentLength(value.toInt());
  } else if (!isConnectionHeader(name)) {
    response_->addHeader(name.str(), value.str());
  }
}

void HttpResponseBuilder::onMessageHeaderEnd() {
}

void HttpResponseBuilder::onMessageContent(const BufferRef& chunk) {
  response_->write(Buffer(chunk));
}

void HttpResponseBuilder::onMessageContent(FileView&& chunk) {
  response_->write(std::move(chunk));
}

void HttpResponseBuilder::onMessageEnd() {
  response_->completed();
}

void HttpResponseBuilder::onError(std::error_code ec) {
  if (ec.category() == HttpStatusCategory::get()) {
    response_->sendError(static_cast<HttpStatus>(ec.value()));
  } else {
    logError("proxy: Unhandled error in response builder. {}", ec.message());
    response_->sendError(HttpStatus::InternalServerError);
  }
}
// }}}

Cluster* ProxyModule::findLocalCluster(const std::string& host) {
  auto i = clusterMap_.find(host);
  if (i != clusterMap_.end())
    return i->second.get();

  std::string path = FileUtil::joinPaths(XZERO_CLUSTERDIR,
                                         host + ".cluster.conf");
  if (!FileUtil::exists(path))
    return nullptr;

  Cluster* cluster = createCluster(host, path);
  cluster->setConfiguration(FileUtil::read(path).str(), path);
  return cluster;
}

bool ProxyModule::proxy_cluster_auto(Context* cx, Params& args) {
  // determines which cluster to use by request host header
  std::string host = cx->request()->getHeader("Host");
  size_t colon = host.find(':');
  if (colon != std::string::npos)
    host = host.substr(0, colon);

  Cluster* cluster = findLocalCluster(host);
  if (!cluster) {
    return cx->sendErrorPage(HttpStatus::NotFound);
  }

  std::string pseudonym = pseudonym_;
  if (pseudonym.empty()) {
    if (const std::optional<InetAddress>& addr = cx->request()->localAddress()) {
      pseudonym = StringUtil::format("{}:{}", addr->ip(), addr->port());
    } else {
      pseudonym = Application::hostname();
    }
  }

  DEBUG("proxy.cluster() auto-detect local cluster '{}', pseudonym '{}'",
      cluster->name(), pseudonym);

  ClusterContext* cc = cx->setCustomData<ClusterContext>(this,
      *cx->request(),
      //cx->request()->getContent(),
      std::make_unique<HttpResponseBuilder>(cx->response()),
      cx->response()->executor(),
      daemon().config().responseBodyBufferSize,
      pseudonym);

  cluster->schedule(cc);

  return true;
}

bool ProxyModule::proxy_cluster(Context* cx, Params& args) {
  auto& cluster = clusterMap_[args.getString(1)];
  std::string path = args.getString(2);
  std::string bucketName = args.getString(3);
  std::string backendName = args.getString(4);

  if (cx->tryServeTraceProxy())
    return true;

  Cluster::RequestShaper::Node* bucket = cluster->rootBucket();
  if (!bucketName.empty()) {
    auto foundBucket = cluster->findBucket(bucketName);
    if (foundBucket) {
      bucket = foundBucket;
    } else {
      logError("proxy: Cluster {} is missing bucket {}. Defaulting to {}",
               cluster->name(), bucketName, bucket->name());
    }
  }

  ClusterContext* cc = cx->setCustomData<ClusterContext>(this,
      *cx->request(),
      // cx->request()->getContentBuffer(),
      std::make_unique<HttpResponseBuilder>(cx->response()),
      cx->response()->executor(),
      daemon().config().responseBodyBufferSize,
      pseudonym_);

  cluster->schedule(cc, bucket);

  return true;
}

bool ProxyModule::proxy_api(Context* cx, flow::Params& args) {
  std::string prefix = args.getString(1);

  if (!StringUtil::beginsWithIgnoreCase(cx->request()->path(), prefix))
    return false;

  ApiHandler* handler = cx->setCustomData<ApiHandler>(
      this, this, cx->request(), cx->response(), prefix);

  return handler->run();
}

bool ProxyModule::proxy_fcgi(Context* cx, flow::Params& args) {
  cx->logError("proxy.fcgi: Not yet reimplemented");
  return false; // TODO
}

bool ProxyModule::proxy_http(Context* cx, flow::Params& args) {
  const FlowString onClientAbortStr = args.getString(1);
  const InetAddress upstreamAddr(args.getIPAddress(2), args.getInt(3));
  const Duration connectTimeout = Duration::fromSeconds(args.getInt(4));
  const Duration readTimeout = Duration::fromSeconds(args.getInt(5));
  const Duration writeTimeout = Duration::fromSeconds(args.getInt(6));
  constexpr Duration keepAlive = 0_seconds;
  Executor* executor = cx->response()->executor();

  // TODO: handle onClientAbortStr == ("close" | "ignore")
  // - if set to close, also close upstream
  // - if set to ignore, finish receiving upstream response

  if (cx->tryServeTraceProxy())
    return true;

  HttpClient* client = cx->setCustomData<HttpClient>(this,
      executor, upstreamAddr,
      connectTimeout, readTimeout, writeTimeout, keepAlive);

  Future<HttpClient::Response> f = client->send(*cx->request());

  f.onFailure([cx, upstreamAddr] (std::error_code ec) {
    // XXX defer execution to ensure we're truely async, to avoid nested runner.
    // TODO: we could instead make resume() a no-op if it's already in the loop.
    cx->response()->executor()->execute([cx, upstreamAddr, ec]() {
      cx->logError("proxy: Failed to proxy to {}. {}", upstreamAddr, ec.message());
      bool internalRedirect = false;
      cx->sendErrorPage(HttpStatus::ServiceUnavailable, &internalRedirect);
      if (internalRedirect) {
        cx->runner()->resume();
      }
    });
  });

  f.onSuccess([cx, this] (HttpClient::Response& response) {
    for (const HeaderField& field: response.headers()) {
      if (!isConnectionHeader(field.name())) {
        cx->response()->addHeader(field.name(), field.value());
      }
    }
    addVia(cx);

    // TODO: what about streaming responses?
    cx->response()->setStatus(response.status());
    cx->response()->setReason(response.reason());
    cx->response()->setContentLength(response.content().size());
    cx->response()->write(std::move(response.content()));
    cx->response()->completed();
  });

  return true;
}

void ProxyModule::addVia(Context* cx) {
  addVia(cx->request(), cx->response());
}

void ProxyModule::addVia(const HttpRequestInfo* in, HttpResponse* out) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%s %s",
           to_string(in->version()).c_str(),
           pseudonym_.c_str());

  // RFC 7230, section 5.7.1: makes it clear, that we put ourselfs into the
  // front of the Via-list.
  out->prependHeader("Via", buf);
}

std::list<Cluster*> ProxyModule::listCluster() {
  // TODO: unordered_map is not thread-safe
  std::list<Cluster*> result;
  for (auto& cluster: clusterMap_)
    result.push_back(cluster.second.get());

  return result;
}

Cluster* ProxyModule::findCluster(const std::string& name) {
  // TODO: unordered_map is not thread-safe
  auto i = clusterMap_.find(name);
  return i != clusterMap_.end() ? i->second.get() : nullptr;
}

Cluster* ProxyModule::createCluster(const std::string& name,
                                        const std::string& path) {
  // TODO: unordered_map is not thread-safe

  // quick-path if invoked again on the same cluster name
  if (clusterMap_.find(name) != clusterMap_.end())
    return clusterMap_[name].get();

  Executor* executor = daemon().selectClientExecutor();
  std::shared_ptr<Cluster> cluster =
      std::make_shared<Cluster>(name, path, executor);
  clusterMap_[name] = cluster;

  if (FileUtil::exists(path)) {
    logInfo("proxy: Loading cluster {} ({})", name, path);
    cluster->setConfiguration(FileUtil::read(path).str(), path);
  } else {
    // auto-create basedir if not present yet
    std::string abspath = FileUtil::absolutePath(path);
    std::string dirname = FileUtil::dirname(abspath);
    FileUtil::mkdir_p(dirname);

    logInfo("proxy: Initializing new cluster {} ({})", name, path);
    cluster->saveConfiguration();
  }
  return cluster.get();
}

void ProxyModule::destroyCluster(const std::string& name) {
  // TODO: naive implementation must be thread-safe
  auto i = clusterMap_.find(name);
  if (i != clusterMap_.end()) {
    clusterMap_.erase(i);
  }
}

bool ProxyModule::proxy_roadwarrior_verify(flow::Instr* instr, flow::IRBuilder* builder) {
  return true; // TODO
}

void ProxyModule::proxy_cache(Context* cx, flow::Params& args) {
  // TODO
}

} // namespace x0d
