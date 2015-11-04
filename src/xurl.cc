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

using namespace xzero;
using namespace xzero::http::client;

using xzero::http::HttpRequestInfo;
using xzero::http::HttpVersion;
using xzero::http::HeaderField;

int main(int argc, const char* argv[]) {
  Application::logToStderr(LogLevel::Info);

  CLI cli;
  cli.defineString("method", 'X', "METHOD", "HTTP method", "GET", nullptr);

  Flags flags = cli.evaluate(argc, argv);

  LocalScheduler sched(std::unique_ptr<ExceptionHandler>(
      new CatchAndLogExceptionHandler("xurl")));

  std::string url = "http://www.google.com/";
  Uri uri(url);

  DnsClient dns;
  std::vector<IPAddress> ipaddresses = dns.ipv4(uri.host());
  if (ipaddresses.empty())
    return 1;
  IPAddress ipaddr = ipaddresses.front();

  int port = uri.port();
  if (!port) {
    if (uri.scheme() == "http")
      port = 80;
    else if (uri.scheme() == "https")
      port = 443;
    else
      return 1;
  }

  Duration connectTimeout = Duration::fromSeconds(4);

  RefPtr<InetEndPoint> ep = InetEndPoint::connect(
      ipaddr, port, connectTimeout, &sched);

  HttpClient http(&sched, ep.as<EndPoint>());

  std::string method = flags.getString("method");

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

  return 0;
}
