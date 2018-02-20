// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
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
#include <x0d/XzeroContext.h>
#include <xzero/sysconfig.h>
#include <xzero/http/proxy/HttpCluster.h>
#include <xzero/http/proxy/HttpClusterRequest.h>
#include <xzero/http/proxy/HttpClusterApiHandler.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/http1/Generator.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/net/TcpEndPoint.h>
#include <xzero/io/FileUtil.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/logging.h>
#include <xzero/Application.h>
#include <xzero-flow/AST.h>
#include <xzero-flow/ir/Instr.h>
#include <xzero-flow/ir/BasicBlock.h>
#include <xzero-flow/ir/IRHandler.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/ConstantValue.h>
#include <xzero-flow/ir/ConstantArray.h>

namespace x0d {

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

using xzero::http::client::HttpClusterApiHandler;
using xzero::http::client::HttpClusterRequest;
using xzero::http::client::HttpCluster;
using xzero::http::client::HttpClient;

#define TRACE(msg...) logTrace("proxy", msg)
#define DEBUG(msg...) logDebug("proxy", msg)

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

ProxyModule::ProxyModule(XzeroDaemon* d)
    : XzeroModule(d, "proxy"),
      pseudonym_("x0d"),
      clusterInit_(),
      clusterMap_() {

  setupFunction("proxy.pseudonym", &ProxyModule::proxy_pseudonym,
                FlowType::String);

  mainHandler("proxy.cluster", &ProxyModule::proxy_cluster_auto);

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
      .param<bool>("enabled", true)
      .param<FlowString>("key", "")
      .param<int>("ttl", 0);
}

ProxyModule::~ProxyModule() {
}

void ProxyModule::proxy_pseudonym(xzero::flow::vm::Params& args) {
  std::string value = args.getString(1).str();

  for (char ch: value) {
    if (!std::isalnum(ch) && ch != '_' && ch != '-' && ch != '.') {
      RAISE(ConfigurationError, "Invalid character found in proxy.pseudonym");
    }
  }

  pseudonym_ = value;
}

bool ProxyModule::verify_proxy_cluster(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder) {
  auto program = call->parent()->parent()->parent();

  auto nameArg = dynamic_cast<ConstantString*>(call->operand(1));
  if (nameArg == nullptr) {
    logError("x0d", "proxy.cluster: name parameter must be a literal.");
    return false;
  }

  if (nameArg->get().empty()) {
    logError("x0d", "Setting empty proxy.cluster name is not allowed.");
    return false;
  }

  // pathArg
  auto pathArg = dynamic_cast<ConstantString*>(call->operand(2));
  if (pathArg == nullptr) {
    logError("x0d", "proxy.cluster: path parameter must be a literal.");
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
  using client::HttpCluster;

  TRACE("clusterInit count: $0", clusterInit_.size());
  for (const auto& init: clusterInit_) {
    TRACE("clusterInit: spawning $0", init.first);
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
    logError("proxy", "Unhandled error in response builder. $0", ec.message());
    response_->sendError(HttpStatus::InternalServerError);
  }
}
// }}}

HttpCluster* ProxyModule::findLocalCluster(const std::string& host) {
  auto i = clusterMap_.find(host);
  if (i != clusterMap_.end())
    return i->second.get();

  std::string path = FileUtil::joinPaths(XZERO_CLUSTERDIR,
                                         host + ".cluster.conf");
  if (!FileUtil::exists(path))
    return nullptr;

  HttpCluster* cluster = createCluster(host, path);
  cluster->setConfiguration(FileUtil::read(path).str(), path);
  return cluster;
}

bool ProxyModule::proxy_cluster_auto(XzeroContext* cx, Params& args) {
  // determines which cluster to use by request host header
  std::string host = cx->request()->getHeader("Host");
  size_t colon = host.find(':');
  if (colon != std::string::npos)
    host = host.substr(0, colon);

  HttpCluster* cluster = findLocalCluster(host);
  if (!cluster) {
    return cx->sendErrorPage(HttpStatus::NotFound);
  }

  std::string pseudonym = pseudonym_;
  if (pseudonym.empty()) {
    if (Option<InetAddress> addr = cx->request()->localAddress()) {
      pseudonym = StringUtil::format("$0:$1", addr->ip(), addr->port());
    } else {
      pseudonym = Application::hostname();
    }
  }

  DEBUG("proxy.cluster() auto-detect local cluster '$0', pseudonym '$1'",
      cluster->name(), pseudonym);

  HttpClusterRequest* cr = cx->setCustomData<HttpClusterRequest>(this,
      *cx->request(),
      //cx->request()->getContent(),
      std::unique_ptr<HttpListener>(new HttpResponseBuilder(cx->response())),
      cx->response()->executor(),
      daemon().config().responseBodyBufferSize,
      pseudonym);

  cluster->schedule(cr);

  return true;
}

bool ProxyModule::proxy_cluster(XzeroContext* cx, Params& args) {
  auto& cluster = clusterMap_[args.getString(1).str()];
  std::string path = args.getString(2).str();
  std::string bucketName = args.getString(3).str();
  std::string backendName = args.getString(4).str();

  if (tryHandleTrace(cx))
    return true;

  TRACE("proxy.cluster: $0", cluster->name());

  HttpCluster::RequestShaper::Node* bucket = cluster->rootBucket();
  if (!bucketName.empty()) {
    auto foundBucket = cluster->findBucket(bucketName);
    if (foundBucket) {
      bucket = foundBucket;
    } else {
      logError("proxy", "Cluster $0 is missing bucket $1. Defaulting to $2",
               cluster->name(), bucketName, bucket->name());
    }
  }

  HttpClusterRequest* cr = cx->setCustomData<HttpClusterRequest>(this,
      *cx->request(),
      // cx->request()->getContentBuffer(),
      std::unique_ptr<HttpListener>(new HttpResponseBuilder(cx->response())),
      cx->response()->executor(),
      daemon().config().responseBodyBufferSize,
      pseudonym_);

  cluster->schedule(cr, bucket);

  return true;
}

bool ProxyModule::proxy_api(XzeroContext* cx, xzero::flow::vm::Params& args) {
  std::string prefix = args.getString(1).str();

  if (!StringUtil::beginsWithIgnoreCase(cx->request()->path(), prefix))
    return false;

  HttpClusterApiHandler* handler = cx->setCustomData<HttpClusterApiHandler>(
      this, this, cx->request(), cx->response(), prefix);

  return handler->run();
}

bool ProxyModule::proxy_fcgi(XzeroContext* cx, xzero::flow::vm::Params& args) {
  cx->logError("proxy.fcgi: Not yet reimplemented");
  return false; // TODO
}

bool ProxyModule::proxy_http(XzeroContext* cx, xzero::flow::vm::Params& args) {
  const FlowString onClientAbortStr = args.getString(1);
  const InetAddress upstreamAddr(args.getIPAddress(2), args.getInt(3));
  const Duration connectTimeout = Duration::fromSeconds(args.getInt(4));
  const Duration readTimeout = Duration::fromSeconds(args.getInt(5));
  const Duration writeTimeout = Duration::fromSeconds(args.getInt(6));
  constexpr Duration keepAlive = 0_seconds;
  Executor* executor = cx->response()->executor();

  if (tryHandleTrace(cx))
    return true;

  HttpClient* client = cx->setCustomData<HttpClient>(this,
      executor, upstreamAddr,
      connectTimeout, readTimeout, writeTimeout, keepAlive);

  Future<HttpClient::Response> f = client->send(*cx->request());

  f.onFailure([cx, upstreamAddr] (std::error_code ec) {
    // XXX defer execution to ensure we're truely async, to avoid nested runner.
    // TODO: we could instead make resume() a no-op if it's already in the loop.
    cx->response()->executor()->execute([cx, upstreamAddr, ec]() {
      cx->logError("proxy: Failed to proxy to $0. $1", upstreamAddr, ec.message());
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

void ProxyModule::addVia(XzeroContext* cx) {
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

std::list<HttpCluster*> ProxyModule::listCluster() {
  // TODO: unordered_map is not thread-safe
  std::list<HttpCluster*> result;
  for (auto& cluster: clusterMap_)
    result.push_back(cluster.second.get());

  return result;
}

HttpCluster* ProxyModule::findCluster(const std::string& name) {
  // TODO: unordered_map is not thread-safe
  auto i = clusterMap_.find(name);
  return i != clusterMap_.end() ? i->second.get() : nullptr;
}

HttpCluster* ProxyModule::createCluster(const std::string& name,
                                        const std::string& path) {
  // TODO: unordered_map is not thread-safe

  // quick-path if invoked again on the same cluster name
  if (clusterMap_.find(name) != clusterMap_.end())
    return clusterMap_[name].get();

  Executor* executor = daemon().selectClientExecutor();
  std::shared_ptr<HttpCluster> cluster(new HttpCluster(name, path, executor));
  clusterMap_[name] = cluster;

  if (FileUtil::exists(path)) {
    logInfo("proxy", "Loading cluster $0 ($1)", name, path);
    cluster->setConfiguration(FileUtil::read(path).str(), path);
  } else {
    logInfo("proxy", "Initializing new cluster $0 ($1)", name, path);
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

bool ProxyModule::proxy_roadwarrior_verify(xzero::flow::Instr* instr, xzero::flow::IRBuilder* builder) {
  return true; // TODO
}

void ProxyModule::proxy_cache(XzeroContext* cx, xzero::flow::vm::Params& args) {
  // TODO
}

bool ProxyModule::tryHandleTrace(XzeroContext* cx) {
  if (cx->request()->method() != HttpMethod::TRACE)
    return false;

  if (!cx->request()->hasHeader("Max-Forwards")) {
    cx->response()->setStatus(HttpStatus::BadRequest);
    cx->response()->setReason("Max-Forwards header missing.");
    cx->response()->completed();
    return true;
  }

  int maxForwards = std::stoi(cx->request()->getHeader("Max-Forwards"));
  if (maxForwards != 0) {
    cx->request()->headers().overwrite("Max-Forwards",
                                       to_string(maxForwards - 1));
    return false;
  }

  BufferRef body = cx->request()->getContent().getBuffer();

  HttpRequestInfo requestInfo(
      cx->request()->version(),
      cx->request()->unparsedMethod(),
      cx->request()->unparsedUri(),
      body.size(),
      cx->request()->headers());
  HeaderFieldList trailers;

  EndPointWriter writer;
  http1::Generator generator(&writer);
  generator.generateRequest(requestInfo);
  generator.generateBody(body);
  generator.generateTrailer(trailers);

  Buffer message;
  writer.flushTo(&message);

  cx->response()->setStatus(HttpStatus::Ok);
  cx->response()->addHeader("Content-Type", "message/http");
  cx->response()->setContentLength(message.size());
  cx->response()->write(std::move(message));
  cx->response()->completed();

  return true;
}

} // namespace x0d
