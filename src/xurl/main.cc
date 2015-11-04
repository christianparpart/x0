#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/executor/LocalScheduler.h>
#include <xzero/Application.h>
#include <xzero/RuntimeError.h>
#include <xzero/net/DnsClient.h>
#include <xzero/Uri.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>
#include <xzero/logging.h>
#include "sysconfig.h"
#include <iostream>
#include <unistd.h>

#define PACKAGE_VERSION X0_VERSION
#define PACKAGE_HOMEPAGE_URL "https://xzero.io"

using namespace xzero;
using namespace xzero::http::client;

using xzero::http::HttpRequestInfo;
using xzero::http::HttpVersion;
using xzero::http::HeaderField;

class XUrl {
 public:
  XUrl();

  int run(int argc, const char* argv[]);

 private:
  void query(const std::string& url);

 private:
  LocalScheduler scheduler_;
  Flags flags_;
  DnsClient dns_;
  Duration connectTimeout_;

};

XUrl::XUrl()
    : scheduler_(std::unique_ptr<ExceptionHandler>(
                    new CatchAndLogExceptionHandler("xurl"))),
      flags_(),
      dns_(),
      connectTimeout_(Duration::fromSeconds(4))
{
  Application::logToStderr(LogLevel::Info);
}

int XUrl::run(int argc, const char* argv[]) {
  CLI cli;
  cli.defineBool("help", 'h', "Prints this help.");
  cli.defineString("output", 'o', "PATH", "Write response body to given file.");
  cli.defineString("method", 'X', "METHOD", "HTTP method", "GET");
  cli.defineNumber("connect-timeout", 0, "MS", "TCP connect() timeout", Duration::fromSeconds(10).milliseconds(), nullptr);
  cli.defineBool("ipv4", '4', "Favor IPv4 for TCP/IP communication.");
  cli.defineBool("ipv6", '6', "Favor IPv6 for TCP/IP communication.");
  cli.enableParameters("URL", "URL to query");

  flags_ = cli.evaluate(argc, argv);

  if (flags_.getBool("help")) {
    std::cerr
      << "xurl: Xzero HTTP Client" PACKAGE_VERSION
          << " [" PACKAGE_HOMEPAGE_URL "]" << std::endl
      << "Copyright (c) 2009-2015 by Christian Parpart <trapni@gmail.com>" << std::endl
      << std::endl
      << "Usage: xurl [options ...]" << std::endl
      << std::endl
      << "Options:" << std::endl
      << cli.helpText() << std::endl;
    return 0;
  }

  if (flags_.parameters().empty()) {
    logError("xurl", "No URL given.");
    return 1;
  }

  for (const std::string& url: flags_.parameters())
    query(url);

  return 0;
}

void XUrl::query(const std::string& url) {
  Uri uri(url);

  if (uri.path() == "")
    uri.setPath("/");

  std::vector<IPAddress> ipaddresses = dns_.ipv4(uri.host());
  if (ipaddresses.empty())
    RAISE(RuntimeError, "Could not resolve %s.", uri.host().c_str());

  IPAddress ipaddr = ipaddresses.front();

  int port = uri.port();
  if (!port) {
    if (uri.scheme() == "http")
      port = 80;
    else if (uri.scheme() == "https")
      port = 443;
    else
      RAISE(RuntimeError, "Unknown scheme '%s'", uri.scheme().c_str());
  }

  RefPtr<InetEndPoint> ep = InetEndPoint::connect(
      ipaddr, port, connectTimeout_, &scheduler_);

  HttpClient http(&scheduler_, ep.as<EndPoint>());

  std::string method = flags_.getString("method");

  http::HeaderFieldList requestHeaders = {
      {"Host", uri.hostAndPort()},
  };
  HttpRequestInfo req(HttpVersion::VERSION_1_1, method, uri.pathAndQuery(), 0,
                      requestHeaders);
  std::string body;

  logInfo("xurl", "$0 $1 HTTP/$2", req.method(), req.entity(), req.version());
  for (const HeaderField& field: req.headers()) {
    logInfo("xurl", "< $0: $1", field.name(), field.value());
  }

  http.send(std::move(req), body);

  logInfo("xurl", "HTTP/$0 $1 $2", http.responseInfo().version(),
                                   (int) http.responseInfo().status(),
                                   http.responseInfo().reason());

  for (const HeaderField& field: http.responseInfo().headers()) {
    logInfo("xurl", "> $0: $1", field.name(), field.value());
  }

  write(1, body.data(), body.size());
}

int main(int argc, const char* argv[]) {
  XUrl app;
  return app.run(argc, argv);
}
