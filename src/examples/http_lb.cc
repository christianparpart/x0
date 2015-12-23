#include <xzero/http/client/HttpCluster.h>
#include <xzero/http/client/HttpClusterRequest.h>
#include <xzero/http/HttpService.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/net/InetAddress.h>
#include <xzero/executor/PosixScheduler.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>
#include <string>
#include <list>

using namespace xzero;
using namespace xzero::http;

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

class LoadBalancer : public HttpService::Handler {
 public:
  explicit LoadBalancer(const InetAddress& serviceAddr);
  void addUpstream(const InetAddress& addr);
  void run();
  bool handleRequest(HttpRequest* request, HttpResponse* response) override;

 private:
  PosixScheduler scheduler_;
  HttpService service_;
  client::HttpCluster cluster_;
};

LoadBalancer::LoadBalancer(const InetAddress& serviceAddr)
    : scheduler_(),
      service_(HttpService::HTTP1),
      cluster_("my-lb", "/tmp", &scheduler_) {
  service_.addHandler(this);

  service_.configureInet(
      &scheduler_, &scheduler_,
      30_seconds, // read timeout
      10_seconds, // write timeout
      8_seconds,  // TCP FIN timeout
      serviceAddr.ip(),
      serviceAddr.port());
}

void LoadBalancer::addUpstream(const InetAddress& addr) {
  cluster_.addMember(addr);
}

void LoadBalancer::run() {
  service_.start();
  scheduler_.runLoop();
}

bool LoadBalancer::handleRequest(HttpRequest* request, HttpResponse* response) {
  // TODO: ensure full request body read

  client::HttpClusterRequest* cr = new client::HttpClusterRequest(
      *request,
      request->getContentBuffer(),
      std::unique_ptr<HttpListener>(new HttpResponseBuilder(response)),
      response->executor(),
      16 * 1024 * 1024, // response body buffer size
      "http_lb"
  );

  // TODO: ensure cr gets deleted, too

  cluster_.schedule(cr);

  return true;
}

int main(int argc, const char* argv[]) {
  CLI cli;

  cli.defineIPAddress("bind", 'b', "IPADDR",
      "IP address to bind listener to.",
      IPAddress("127.0.0.1"),
      nullptr);

  cli.defineNumber("port", 'p', "PORT",
      "Port number to listen on.",
      3000,
      nullptr);

  std::list<std::string> upstreams;
  cli.defineString("upstream", 'u', "IP:PORT",
      "Upstream to proxy to.",
      [&](const std::string& arg) { upstreams.push_back(arg); });

  Flags flags = cli.evaluate(argc, argv);

  LoadBalancer lb(InetAddress(flags.getIPAddress("bind"), flags.getNumber("port")));

  for (const std::string& upstream: upstreams)
    lb.addUpstream(InetAddress(upstream));

  lb.run();

  return 0;
}
