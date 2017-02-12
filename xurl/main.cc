// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero/http/client/HttpClient.h>
#include <xzero/http/HttpRequestInfo.h>
#include <xzero/http/HeaderFieldList.h>
#include <xzero/net/InetEndPoint.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/net/DnsClient.h>
#include <xzero/io/FileUtil.h>
#include <xzero/Application.h>
#include <xzero/RuntimeError.h>
#include <xzero/Uri.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>
#include <xzero/logging.h>
#include <unordered_map>
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
  void connected(RefPtr<EndPoint> ep, const Uri& uri);
  void connectFailure(Status error);

 private:
  NativeScheduler scheduler_;
  Flags flags_;
  DnsClient dns_;
  Duration connectTimeout_;
  Duration readTimeout_;
  Duration writeTimeout_;
  http::HeaderFieldList requestHeaders_;
  Buffer body_;
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
}

void XUrl::addRequestHeader(const std::string& field) {
  requestHeaders_.push_back(HeaderField::parse(field));
}

int XUrl::run(int argc, const char* argv[]) {
  CLI cli;
  cli.defineBool("help", 'h', "Prints this help.");
  cli.defineBool("head", 'I', "Performs a HEAD request");
  cli.defineString("output", 'o', "PATH", "Write response body to given file.");
  cli.defineString("log-level", 0, "STRING", "Log level.", "info");
  cli.defineString("method", 'X', "METHOD", "HTTP method", "GET");
  cli.defineNumber("connect-timeout", 0, "MS", "TCP connect() timeout", 10_seconds .milliseconds(), nullptr);
  cli.defineString("upload-file", 'T', "PATH", "Uploads given file.", "");
  cli.defineString("header", 'H', "HEADER", "Adds a custom request header",
      std::bind(&XUrl::addRequestHeader, this, std::placeholders::_1));
  cli.defineBool("ipv4", '4', "Favor IPv4 for TCP/IP communication.");
  cli.defineBool("ipv6", '6', "Favor IPv6 for TCP/IP communication.");
  cli.enableParameters("URL", "URL to query");

  flags_ = cli.evaluate(argc, argv);

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
      << cli.helpText() << std::endl;
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
  InetAddress addr(ipaddr, port);

  InetEndPoint::connectAsync(
      addr, connectTimeout_, readTimeout_, writeTimeout_, &scheduler_,
      std::bind(&XUrl::connected, this, std::placeholders::_1, uri),
      std::bind(&XUrl::connectFailure, this, std::placeholders::_1));

  scheduler_.runLoop();
}

void XUrl::connected(RefPtr<EndPoint> ep, const Uri& uri) {
  HttpClient http(&scheduler_);

  std::string method = flags_.getString("method");
  if (flags_.getBool("head")) {
    method = "HEAD";
  }

  if (!flags_.getString("upload-file").empty()) {
    method = "PUT";
    body_ = FileUtil::read(flags_.getString("upload-file"));
  }

  requestHeaders_.overwrite("Host", uri.hostAndPort());
  HttpRequestInfo req(HttpVersion::VERSION_1_1,
                      method,
                      uri.pathAndQuery(),
                      body_.size(),
                      requestHeaders_);

  logInfo("xurl", "$0 $1 HTTP/$2",
          req.unparsedMethod(), req.unparsedUri(), req.version());

  for (const HeaderField& field: req.headers()) {
    logInfo("xurl", "< $0: $1", field.name(), field.value());
  }

  http.setRequest(req, body_);
  http.send(ep);

  scheduler_.runLoop();

  logInfo("xurl", "HTTP/$0 $1 $2", http.responseInfo().version(),
                                   (int) http.responseInfo().status(),
                                   http.responseInfo().reason());

  for (const HeaderField& field: http.responseInfo().headers()) {
    logInfo("xurl", "> $0: $1", field.name(), field.value());
  }

  write(1, http.responseBody().data(), http.responseBody().size());
}

void XUrl::connectFailure(Status error) {
  logError("xurl", "connect() failed. $0", error);
}

int main(int argc, const char* argv[]) {
  XUrl app;
  return app.run(argc, argv);
}
