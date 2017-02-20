// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

/*
 * Noteworthy
 *
 * - does not forward Expect request header to upstream
 */

#include "proxy.h"
#include <x0d/XzeroContext.h>
#include <xzero/sysconfig.h>
#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClusterApiHandler.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/http1/Generator.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/net/EndPointWriter.h>
#include <xzero/net/ByteArrayEndPoint.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/BufferInputStream.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/BufferUtil.h>
#include <xzero/logging.h>
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

using xzero::http::client::HttpClusterApiHandler;
using xzero::http::client::HttpClusterRequest;
using xzero::http::client::HttpCluster;
using xzero::http::client::HttpClient;

#define TRACE(msg...) logTrace("proxy", msg)

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
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<FlowString>("on_client_abort", "close");

  mainHandler("proxy.http", &ProxyModule::proxy_http)
      .verifier(&ProxyModule::proxy_roadwarrior_verify, this)
      .param<IPAddress>("address")
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
  void onProtocolError(HttpStatus code, const std::string& message) override;

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

void HttpResponseBuilder::onProtocolError(HttpStatus code, const std::string& message) {
  // TODO used? sufficient? resistent? chocolate?
  response_->setStatus(code);
  response_->setReason(message);
  response_->completed();
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
    cx->response()->setStatus(HttpStatus::NotFound);
    cx->response()->completed();
    return true;
  }

  std::string pseudonym = pseudonym_;
  if (pseudonym.empty()) {
    pseudonym = StringUtil::format("$0:$1",
        cx->request()->remoteAddress().get().ip(),
        cx->request()->remoteAddress().get().port());
  }

  HttpClusterRequest* cr = cx->setCustomData<HttpClusterRequest>(this,
      *cx->request(),
      cx->request()->getContentBuffer(),
      std::unique_ptr<HttpListener>(new HttpResponseBuilder(cx->response())),
      cx->response()->executor(),
      daemon().config().responseBodyBufferSize,
      pseudonym_);

  cluster->schedule(cr, nullptr);

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
      cx->request()->getContentBuffer(),
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
  return false; // TODO
}

bool ProxyModule::proxy_http(XzeroContext* cx, xzero::flow::vm::Params& args) {
  InetAddress addr(args.getIPAddress(1), args.getInt(2));
  FlowString onClientAbortStr = args.getString(3);
  Duration connectTimeout = 16_seconds;
  Duration readTimeout = 60_seconds;
  Duration writeTimeout = 8_seconds;
  Executor* executor = cx->response()->executor();

  if (tryHandleTrace(cx))
    return true;

  HttpClient* client = new HttpClient(executor);
  client->setRequest(*cx->request(), cx->request()->getContentBuffer());
  Future<HttpClient*> f = client->sendAsync(
      addr, connectTimeout, readTimeout, writeTimeout);

  f.onFailure([cx, addr] (const std::error_code& ec) {
    logError("proxy", "Failed to proxy to $0. $1", addr, ec.message());
    cx->response()->setStatus(HttpStatus::ServiceUnavailable);
    cx->response()->completed();
  });
  f.onSuccess([this, cx] (HttpClient* client) {
    for (const HeaderField& field: client->responseInfo().headers()) {
      if (!isConnectionHeader(field.name())) {
        cx->response()->addHeader(field.name(), field.value());
      }
    }
    addVia(cx);

    cx->response()->setStatus(client->responseInfo().status());
    cx->response()->setReason(client->responseInfo().reason());
    cx->response()->setContentLength(client->responseInfo().contentLength());

    if (client->isResponseBodyBuffered())
      cx->response()->write(client->responseBody());
    else
      cx->response()->write(client->takeResponseBody());

    cx->response()->completed();
    delete client;
  });

  return true;
}

void ProxyModule::addVia(XzeroContext* cx) {
  addVia(cx->request(), cx->response());
}

void ProxyModule::addVia(const HttpRequestInfo* in, HttpResponse* out) {
  char buf[128];
  snprintf(buf, sizeof(buf), "%s %s",
           StringUtil::toString(in->version()).c_str(),
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

bool ProxyModule::proxy_haproxy_monitor(XzeroContext* cx, xzero::flow::vm::Params& args) {
  return false; // TODO
}

bool ProxyModule::proxy_haproxy_stats(XzeroContext* cx, xzero::flow::vm::Params& args) {
  return false; // TODO
}

bool ProxyModule::proxy_roadwarrior_verify(xzero::flow::Instr* instr, xzero::flow::IRBuilder* builder) {
  return true; // TODO
}

void ProxyModule::proxy_cache_enabled(XzeroContext* cx, xzero::flow::vm::Params& args) {
}

void ProxyModule::proxy_cache_key(XzeroContext* cx, xzero::flow::vm::Params& args) {
}

void ProxyModule::proxy_cache_ttl(XzeroContext* cx, xzero::flow::vm::Params& args) {
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
                                       StringUtil::toString(maxForwards - 1));
    return false;
  }

  BufferRef body = cx->request()->getContentBuffer();

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

  ByteArrayEndPoint ep;
  writer.flush(&ep);

  Buffer message = ep.output();

  cx->response()->setStatus(HttpStatus::Ok);
  cx->response()->addHeader("Content-Type", "message/http");
  cx->response()->setContentLength(message.size());
  cx->response()->write(std::move(message));
  cx->response()->completed();

  return true;
}

} // namespace x0d
