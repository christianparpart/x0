// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

/*
 * Noteworthy
 *
 * - does not forward Expect request header to upstream
 */

#include "proxy.h"
#include "XzeroContext.h"
#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/io/FileUtil.h>
#include <xzero/io/BufferInputStream.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/io/FileUtil.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
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

using xzero::http::client::HttpClusterRequest;
using xzero::http::client::HttpCluster;
using xzero::http::client::HttpClient;


#define TRACE(msg...) logTrace("proxy", msg)

ProxyModule::ProxyModule(XzeroDaemon* d)
    : XzeroModule(d, "proxy"),
      pseudonym_("x0d"),
      clusterInit_(),
      clusterMap_() {

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

bool ProxyModule::verify_proxy_cluster(xzero::flow::Instr* call) {
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
      ? "/var/tmp/" + nameArg->get() + ".cluster.conf"
      : pathArg->get();

  call->setOperand(2, program->get(path));

  clusterInit_[nameArg->get()] = path;

  return true;
}

void ProxyModule::onPostConfig() {
  using client::HttpCluster;

  TRACE("clusterInit count: $0", clusterInit_.size());
  for (const auto& init: clusterInit_) {
    TRACE("clusterInit: $0", init.first);
    std::string name = init.first;
    std::string path = init.second;
    Executor* executor = daemon().selectClientScheduler();

    TRACE("Initializing cluster $0. $1", name, path);

    std::shared_ptr<HttpCluster> cluster(new HttpCluster(name, executor));

    if (FileUtil::exists(path)) {
      cluster->setConfiguration(FileUtil::read(path).str());
    } else {
      cluster->setHealthCheckUri(Uri("http://xzero.io/hello.txt"));
      cluster->setHealthCheckInterval(Duration::fromSeconds(10));
      cluster->setHealthCheckSuccessThreshold(1);
      cluster->setHealthCheckSuccessCodes({HttpStatus::Ok});
      cluster->addMember("demo1", IPAddress("127.0.0.1"), 3001, 1, true);
      cluster->setEnabled(true);
    }

    clusterMap_[name] = cluster;
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
  // TODO: skip connection-level headers
  response_->headers().push_back(name.str(), value.str());
}

void HttpResponseBuilder::onMessageHeaderEnd() {
}

void HttpResponseBuilder::onMessageContent(const BufferRef& chunk) {
  response_->output()->write(chunk);
}

void HttpResponseBuilder::onMessageEnd() {
  response_->completed();
}

void HttpResponseBuilder::onProtocolError(HttpStatus code, const std::string& message) {
}
// }}}
class HttpInputStream : public InputStream { // {{{
 public:
  explicit HttpInputStream(HttpInput* input);
  size_t read(Buffer* target, size_t n) override;
  size_t transferTo(OutputStream* target) override;

 private:
  HttpInput* input_;
};

HttpInputStream::HttpInputStream(HttpInput* input)
    : input_(input) {
}

size_t HttpInputStream::read(Buffer* target, size_t n) {
  size_t oldsize = target->size();
  target->reserve(target->size() + n);
  input_->read(target);
  return target->size() - oldsize;
}

size_t HttpInputStream::transferTo(OutputStream* target) {
  return 0; // TODO (unused, actually)
}

// }}}

bool ProxyModule::proxy_cluster(XzeroContext* cx, Params& args) {
  auto& cluster = clusterMap_[args.getString(1).str()];
  std::string path = args.getString(2).str();
  std::string bucketName = args.getString(3).str();
  std::string backendName = args.getString(4).str();

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
      std::unique_ptr<InputStream>(new HttpInputStream(cx->request()->input())),
      std::unique_ptr<HttpListener>(new HttpResponseBuilder(cx->response())),
      cx->response()->executor());

  cluster->schedule(cr, bucket);

  return true;
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

static HeaderFieldList filter(const HeaderFieldList& list,
                              std::function<bool(const HeaderField&)> test) {
  HeaderFieldList result;

  for (const HeaderField& field: list)
    if (test(field))
      result.push_back(field);

  return result;
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
  };

  for (const auto& test: connectionHeaderFields)
    if (iequals(name, test))
      return true;

  return false;
}

auto skipConnectFields = [](const HeaderField& f) -> bool {
  static const std::vector<std::string> connectionHeaderFields = {
    "Connection",
    "Content-Length",
    "Expect",
    "Trailer",
    "Transfer-Encoding",
    "Upgrade",
  };

  for (const auto& name: connectionHeaderFields)
    if (iequals(name, f.name()))
      return false;

  return true;
};

bool ProxyModule::proxy_http(XzeroContext* cx, xzero::flow::vm::Params& args) {
  IPAddress ipaddr = args.getIPAddress(1);
  FlowNumber port = args.getInt(2);
  FlowString onClientAbortStr = args.getString(3);
  Duration connectTimeout = Duration::fromSeconds(16);
  Duration readTimeout = Duration::fromSeconds(60);
  Duration writeTimeout = Duration::fromSeconds(8);
  Executor* executor = cx->response()->executor();

  BufferRef requestBody; // TODO
  size_t requestBodySize = requestBody.size();
  HttpRequestInfo requestInfo(
      HttpVersion::VERSION_1_1,
      cx->request()->unparsedMethod(),
      cx->request()->unparsedUri(),
      requestBodySize,
      filter(cx->request()->headers(), skipConnectFields));

  Future<UniquePtr<HttpClient>> f = HttpClient::sendAsync(
      ipaddr, port, requestInfo, requestBody,
      connectTimeout, readTimeout, writeTimeout, executor);

  f.onFailure([cx, ipaddr, port] (Status s) {
    logError("proxy", "Failed to proxy to $0:$1. $2", ipaddr, port, s);
    cx->response()->setStatus(HttpStatus::ServiceUnavailable);
    cx->response()->completed();
  });
  f.onSuccess([this, cx] (const UniquePtr<HttpClient>& client) {
    for (const HeaderField& field: client->responseInfo().headers()) {
      if (!isConnectionHeader(field.name())) {
        cx->response()->headers().push_back(field.name(), field.value());
      }
    }
    addVia(cx);
    cx->response()->setStatus(client->responseInfo().status());
    cx->response()->setReason(client->responseInfo().reason());
    cx->response()->setContentLength(client->responseBody().size());
    cx->response()->output()->write(client->responseBody());
    cx->response()->completed();
  });

  return true;
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
