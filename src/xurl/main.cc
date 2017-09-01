// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/net/DnsClient.h>
#include <xzero/io/FileUtil.h>
#include <xzero/Application.h>
#include <xzero/RuntimeError.h>
#include <xzero/Uri.h>
#include <xzero/Flags.h>
#include <xzero/logging.h>
#include <unordered_map>
#include "sysconfig.h"
#include <iostream>
#include <unistd.h>

// #define PACKAGE_VERSION X0_VERSION
#define PACKAGE_HOMEPAGE_URL "https://xzero.io"

#define DEBUG(msg...) logDebug("xurl", msg)
#define TRACE(msg...) logTrace("xurl", msg)

using namespace xzero;
using namespace xzero::http;
using namespace xzero::http::client;

using xzero::http::HttpRequestInfo;
using xzero::http::HttpVersion;
using xzero::http::HeaderField;

class ServicePortMapping { // {{{
 public:
  ServicePortMapping();

  void loadFile(const std::string& path = "/etc/services");
  void loadContent(const std::string& content);

  int tcp(const std::string& name);
  int udp(const std::string& name);

 private:
  std::unordered_map<std::string, int> tcp_;
  std::unordered_map<std::string, int> udp_;
};

ServicePortMapping::ServicePortMapping()
    : tcp_() {
  tcp_["http"] = 80;
  tcp_["https"] = 443;
}

int ServicePortMapping::tcp(const std::string& name) {
  auto i = tcp_.find(name);
  if (i != tcp_.end())
    return i->second;

  RAISE(RuntimeError, "Unknown service '%s'", name.c_str());
}
// }}}

class XUrl {
 public:
  XUrl();

  int run(int argc, const char* argv[]);

 private:
  void addRequestHeader(const std::string& value);
  Uri makeUri(const std::string& url);
  IPAddress getIPAddress(const std::string& host);
  int getPort(const Uri& uri);
  void query(const Uri& uri);

 private:
  NativeScheduler scheduler_;
  Flags flags_;
  DnsClient dns_;
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;
  http::HeaderFieldList requestHeaders_;
};

XUrl::XUrl()
    : scheduler_(std::unique_ptr<ExceptionHandler>(
                    new CatchAndLogExceptionHandler("xurl"))),
      flags_(),
      dns_(),
      connectTimeout_(4_seconds),
      readTimeout_(60_seconds),
      writeTimeout_(10_seconds)
{
  Application::logToStderr(LogLevel::Info);

  requestHeaders_.push_back("User-Agent", "xurl/" PACKAGE_VERSION);

  flags_.defineBool("help", 'h', "Prints this help.");
  flags_.defineBool("head", 'I', "Performs a HEAD request");
  flags_.defineString("output", 'o', "PATH", "Write response body to given file.");
  flags_.defineString("log-level", 0, "STRING", "Log level.", "info");
  flags_.defineString("method", 'X', "METHOD", "HTTP method", "GET");
  flags_.defineNumber("connect-timeout", 0, "MS", "TCP connect() timeout", 10_seconds .milliseconds());
  flags_.defineString("upload-file", 'T', "PATH", "Uploads given file.", "");
  flags_.defineString("header", 'H', "HEADER", "Adds a custom request header",
      None(),
      std::bind(&XUrl::addRequestHeader, this, std::placeholders::_1));
  flags_.defineBool("ipv4", '4', "Favor IPv4 for TCP/IP communication.");
  flags_.defineBool("ipv6", '6', "Favor IPv6 for TCP/IP communication.");
  flags_.enableParameters("URL", "URL to query");
}

void XUrl::addRequestHeader(const std::string& field) {
  requestHeaders_.push_back(HeaderField::parse(field));
}

int XUrl::run(int argc, const char* argv[]) {
  std::error_code ec = flags_.parse(argc, argv);
  if (ec) {
    fprintf(stderr, "Failed to parse flags. %s\n", ec.message().c_str());
    return 1;
  }

  Logger::get()->setMinimumLogLevel(make_loglevel(flags_.getString("log-level")));

  if (flags_.getBool("help")) {
    std::cerr
      << "xurl: Xzero HTTP Client" PACKAGE_VERSION
          << " [" PACKAGE_HOMEPAGE_URL "]" << std::endl
      << "Copyright (c) 2009-2017 by Christian Parpart <christian@parpart.family>" << std::endl
      << std::endl
      << "Usage: xurl [options ...]" << std::endl
      << std::endl
      << "Options:" << std::endl
      << flags_.helpText() << std::endl;
    return 0;
  }

  if (flags_.parameters().empty()) {
    logError("xurl", "No URL given.");
    return 1;
  }

  if (flags_.parameters().size() != 1) {
    logError("xurl", "Too many URLs given.");
  }

  Uri uri = makeUri(flags_.parameters()[0]);
  query(uri);

  return 0;
}

Uri XUrl::makeUri(const std::string& url) {
  Uri uri(url);

  if (uri.path() == "")
    uri.setPath("/");

  return uri;
}

IPAddress XUrl::getIPAddress(const std::string& host) {
  std::vector<IPAddress> ipaddresses = dns_.ipv4(host);
  if (ipaddresses.empty())
    RAISE(RuntimeError, "Could not resolve %s.", host.c_str());

  return ipaddresses.front();
}

int XUrl::getPort(const Uri& uri) {
  if (uri.port())
    return uri.port();

  return ServicePortMapping().tcp(uri.scheme());
}

void XUrl::query(const Uri& uri) {
  IPAddress ipaddr = getIPAddress(uri.host());
  int port = getPort(uri);
  InetAddress inetAddr(ipaddr, port);
  Duration keepAlive = 8_seconds;

  const bool secure = false; // HTTP ?

  TRACE("getting request method");
  std::string method = flags_.getString("method");

  TRACE("getting request head?");
  if (flags_.getBool("head")) {
    method = "HEAD";
  }

  TRACE("getting request body");
  HugeBuffer body;
  if (!flags_.getString("upload-file").empty()) {
    method = "PUT";
    body = FileUtil::read(flags_.getString("upload-file"));
  }

  requestHeaders_.overwrite("Host", uri.hostAndPort());

  HttpRequest req(HttpVersion::VERSION_1_1,
                  method,
                  uri.pathAndQuery(),
                  requestHeaders_,
                  secure,
                  std::move(body));

  logInfo("xurl", "$0 $1 HTTP/$2",
          req.unparsedMethod(), req.unparsedUri(), req.version());

  for (const HeaderField& field: req.headers()) {
    logInfo("xurl", "< $0: $1", field.name(), field.value());
  }

  HttpClient httpClient(
      &scheduler_, inetAddr,
      connectTimeout_, readTimeout_, writeTimeout_,
      keepAlive);

  Future<HttpClient::Response> f = httpClient.send(req);

  f.onSuccess([](HttpClient::Response& response) {
    logInfo("xurl", "HTTP/$0 $1 $2", response.version(),
                                     (int) response.status(),
                                     response.reason());

    for (const HeaderField& field: response.headers()) {
      logInfo("xurl", "> $0: $1", field.name(), field.value());
    }

    const BufferRef& content = response.content().getBuffer();
    write(STDOUT_FILENO, content.data(), content.size());
  });

  f.onFailure([](std::error_code ec) {
    logError("xurl", "connect() failed. $0", ec.message());
  });

  scheduler_.runLoop();
}

int main(int argc, const char* argv[]) {
  XUrl app;
  return app.run(argc, argv);
}
