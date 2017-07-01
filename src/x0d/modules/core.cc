// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "core.h"
#include <x0d/XzeroModule.h>
#include <x0d/XzeroContext.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/io/FileUtil.h>
#include <xzero/Application.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/Tokenizer.h>
#include <sstream>
#include <cmath>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>

using namespace xzero;
using namespace xzero::http;
using namespace xzero::flow;

/**
 * * max_connections:
 *
 * * max_read_idle:
 * * max_write_idle:
 * * tcp_cork
 * * tcp_nodelay
 * * lingering
 * * max_request_uri_size
 * * max_request_header_size
 * * max_request_header_count
 * * max_request_body_size
 * * request_header_buffer_size
 * * request_body_buffer_size
 * * http(1)_read_buffer_size: ConnectionFactory::inputBufferSize
 */

namespace x0d {

static inline const char* rc2str(int resource) { // {{{
  switch (resource) {
    case RLIMIT_CORE:
      return "core";
    case RLIMIT_AS:
      return "address-space";
    case RLIMIT_NOFILE:
      return "filedes";
    default:
      return "unknown";
  }
} // }}}

unsigned long long CoreModule::setrlimit(
    int resource, unsigned long long value) {
  struct rlimit rlim;
  if (::getrlimit(resource, &rlim) == -1) {
    logWarning("x0d",
               "Failed to retrieve current resource limit on $0 ($1).",
               rc2str(resource), resource);

    return 0;
  }

  rlim_t last = rlim.rlim_cur;

  // patch against human readable form
  long long hlast = last, hvalue = value;

  if (value > RLIM_INFINITY) value = RLIM_INFINITY;

  rlim.rlim_cur = value;
  rlim.rlim_max = value;

  if (::setrlimit(resource, &rlim) == -1) {
    logWarning("x0d",
               "Failed to set resource limit on $0 from $1 to $2.",
               rc2str(resource), hlast, hvalue);

    return 0;
  }

  logTrace("x0d",
           "Set resource limit on $0 from $1 to $2.",
           rc2str(resource), hlast, hvalue);

  return value;
}

size_t CoreModule::cpuCount() {
  static int numCPU_ = -1;

  if (numCPU_ < 0) {
    numCPU_ = sysconf(_SC_NPROCESSORS_ONLN);
    if (numCPU_ < 0) {
      logError("Could not retrieve processor count. $0", strerror(errno));
      numCPU_ = 1;
    }
  }

  return static_cast<size_t>(numCPU_);
}

CoreModule::CoreModule(XzeroDaemon* d)
    : XzeroModule(d, "core"),
      rng_() {

  // setup functions
  setupFunction("listen", &CoreModule::listen)
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<int>("backlog", 0) // <= 0 means, it'll default to system-default
      .param<int>("multi_accept", 1)
      .param<bool>("defer_accept", false)
      .param<bool>("reuse_port", false);

  setupFunction("ssl.listen", &CoreModule::ssl_listen)
      .param<IPAddress>("address", IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<int>("backlog", 0) // <= 0 means, it'll default to system-default
      .param<int>("multi_accept", 1)
      .param<bool>("defer_accept", false)
      .param<bool>("reuse_port", false);

  setupFunction("ssl.context", &CoreModule::ssl_context)
      .param<flow::FlowString>("keyfile")
      .param<flow::FlowString>("certfile")
      .param<flow::FlowString>("trustfile", "")
      .param<flow::FlowString>("priorities", "");

  setupFunction("ssl.priorities", &CoreModule::ssl_priorities, flow::FlowType::String);

  // setup: properties (write-only)
  setupFunction("workers", &CoreModule::workers, FlowType::Number);
  setupFunction("workers", &CoreModule::workers_affinity, FlowType::IntArray);
  setupFunction("mimetypes", &CoreModule::mimetypes, FlowType::String);
  setupFunction("mimetypes.default", &CoreModule::mimetypes_default, FlowType::String);
  setupFunction("etag.mtime", &CoreModule::etag_mtime, FlowType::Boolean);
  setupFunction("etag.size", &CoreModule::etag_size, FlowType::Boolean);
  setupFunction("etag.inode", &CoreModule::etag_inode, FlowType::Boolean);
  setupFunction("fileinfo.ttl", &CoreModule::fileinfo_cache_ttl, FlowType::Number);
  setupFunction("server.advertise", &CoreModule::server_advertise, FlowType::Boolean);
  setupFunction("server.tags", &CoreModule::server_tags, FlowType::StringArray, FlowType::String);
  setupFunction("tcp_fin_timeout", &CoreModule::tcp_fin_timeout, FlowType::Number);
  setupFunction("max_internal_redirect_count", &CoreModule::max_internal_redirect_count, FlowType::Number);
  setupFunction("max_read_idle", &CoreModule::max_read_idle, FlowType::Number);
  setupFunction("max_write_idle", &CoreModule::max_write_idle, FlowType::Number);
  setupFunction("max_keepalive_idle", &CoreModule::max_keepalive_idle, FlowType::Number);
  setupFunction("max_keepalive_requests", &CoreModule::max_keepalive_requests, FlowType::Number);
  setupFunction("max_connections", &CoreModule::max_conns, FlowType::Number);
  setupFunction("max_files", &CoreModule::max_files, FlowType::Number);
  setupFunction("max_address_space", &CoreModule::max_address_space, FlowType::Number);
  setupFunction("max_core_size", &CoreModule::max_core, FlowType::Number);
  setupFunction("tcp_cork", &CoreModule::tcp_cork, FlowType::Boolean);
  setupFunction("tcp_nodelay", &CoreModule::tcp_nodelay, FlowType::Boolean);
  setupFunction("lingering", &CoreModule::lingering, FlowType::Number);
  setupFunction("max_request_uri_size", &CoreModule::max_request_uri_size, FlowType::Number);
  setupFunction("max_request_header_size", &CoreModule::max_request_header_size, FlowType::Number);
  setupFunction("max_request_header_count", &CoreModule::max_request_header_count, FlowType::Number);
  setupFunction("max_request_body_size", &CoreModule::max_request_body_size, FlowType::Number);
  setupFunction("request_header_buffer_size", &CoreModule::request_header_buffer_size, FlowType::Number);
  setupFunction("request_body_buffer_size", &CoreModule::request_body_buffer_size, FlowType::Number);
  setupFunction("response_body_buffer_size", &CoreModule::response_body_buffer_size, FlowType::Number);

  // TODO setup error-documents

  // shared properties (read-only)
  sharedFunction("sys.cpu_count", &CoreModule::sys_cpu_count)
      .returnType(FlowType::Number);
  sharedFunction("sys.env", &CoreModule::sys_env, FlowType::String)
      .returnType(FlowType::String)
      .verifier(&CoreModule::preproc_sys_env, this);
  sharedFunction("sys.env", &CoreModule::sys_env2, FlowType::String, FlowType::String)
      .returnType(FlowType::String)
      .verifier(&CoreModule::preproc_sys_env2, this);
  sharedFunction("sys.cwd", &CoreModule::sys_cwd)
      .returnType(FlowType::String);
  sharedFunction("sys.pid", &CoreModule::sys_pid)
      .returnType(FlowType::String);
  sharedFunction("sys.now", &CoreModule::sys_now)
      .returnType(FlowType::String);
  sharedFunction("sys.now_str", &CoreModule::sys_now_str)
      .returnType(FlowType::String);
  sharedFunction("sys.hostname", &CoreModule::sys_hostname)
      .returnType(FlowType::String);
  sharedFunction("sys.domainname", &CoreModule::sys_domainname)
      .returnType(FlowType::String);

  // shared functions
  sharedFunction("error.page", &CoreModule::error_page,
                               &CoreModule::error_page)
      .param<FlowNumber>("status")
      .param<FlowString>("uri");
  sharedFunction("file.exists", &CoreModule::file_exists, FlowType::String)
      .returnType(FlowType::Boolean);
  sharedFunction("file.is_reg", &CoreModule::file_is_reg, FlowType::String)
      .returnType(FlowType::Boolean);
  sharedFunction("file.is_dir", &CoreModule::file_is_dir, FlowType::String)
      .returnType(FlowType::Boolean);
  sharedFunction("file.is_exe", &CoreModule::file_is_exe, FlowType::String)
      .returnType(FlowType::Boolean);
  sharedFunction("log.err", &CoreModule::log_err, FlowType::String);
  sharedFunction("log.warn", &CoreModule::log_warn, FlowType::String);
  sharedFunction("log.notice", &CoreModule::log_notice, FlowType::String);
  sharedFunction("log", &CoreModule::log_info, FlowType::String);
  sharedFunction("log.info", &CoreModule::log_info, FlowType::String);
  sharedFunction("log.debug", &CoreModule::log_debug, FlowType::String);
  sharedFunction("sleep", &CoreModule::sleep, FlowType::Number);
  sharedFunction("rand", &CoreModule::rand)
      .returnType(FlowType::Number);
  sharedFunction("rand", &CoreModule::randAB, FlowType::Number, FlowType::Number)
      .returnType(FlowType::Number);

  // main: read-only attributes
  mainFunction("req.method", &CoreModule::req_method)
      .returnType(FlowType::String);
  mainFunction("req.url", &CoreModule::req_url)
      .returnType(FlowType::String);
  mainFunction("req.path", &CoreModule::req_path)
      .returnType(FlowType::String);
  mainFunction("req.query", &CoreModule::req_query)
      .returnType(FlowType::String);
  mainFunction("req.header", &CoreModule::req_header, FlowType::String)
      .returnType(FlowType::String);
  mainFunction("req.cookie", &CoreModule::req_cookie, FlowType::String)
      .returnType(FlowType::String);
  mainFunction("req.host", &CoreModule::req_host)
      .returnType(FlowType::String);
  mainFunction("req.pathinfo", &CoreModule::req_pathinfo)
      .returnType(FlowType::String);
  mainFunction("req.is_secure", &CoreModule::req_is_secure)
      .returnType(FlowType::Boolean);
  mainFunction("req.scheme", &CoreModule::req_scheme)
      .returnType(FlowType::String);
  mainFunction("req.status", &CoreModule::req_status_code)
      .returnType(FlowType::Number);
  mainFunction("req.remoteip", &CoreModule::conn_remote_ip)
      .returnType(FlowType::IPAddress);
  mainFunction("req.remoteport", &CoreModule::conn_remote_port)
      .returnType(FlowType::Number);
  mainFunction("req.localip", &CoreModule::conn_local_ip)
      .returnType(FlowType::IPAddress);
  mainFunction("req.localport", &CoreModule::conn_local_port)
      .returnType(FlowType::Number);
  mainFunction("phys.path", &CoreModule::phys_path)
      .returnType(FlowType::String);
  mainFunction("phys.exists", &CoreModule::phys_exists)
      .returnType(FlowType::Boolean);
  mainFunction("phys.is_reg", &CoreModule::phys_is_reg)
      .returnType(FlowType::Boolean);
  mainFunction("phys.is_dir", &CoreModule::phys_is_dir)
      .returnType(FlowType::Boolean);
  mainFunction("phys.is_exe", &CoreModule::phys_is_exe)
      .returnType(FlowType::Boolean);
  mainFunction("phys.mtime", &CoreModule::phys_mtime)
      .returnType(FlowType::Number);
  mainFunction("phys.size", &CoreModule::phys_size)
      .returnType(FlowType::Number);
  mainFunction("phys.etag", &CoreModule::phys_etag)
      .returnType(FlowType::String);
  mainFunction("phys.mimetype", &CoreModule::phys_mimetype)
      .returnType(FlowType::String);

  // main: getter functions
  mainFunction("req.accept_language", &CoreModule::req_accept_language, FlowType::StringArray)
      .returnType(FlowType::String)
      .verifier(&CoreModule::verify_req_accept_language, this);
  mainFunction("regex.group", &CoreModule::regex_group, FlowType::Number)
      .returnType(FlowType::String);

  // main: manipulation functions
  mainFunction("header.add", &CoreModule::header_add, FlowType::String, FlowType::String);
  mainFunction("header.append", &CoreModule::header_append)
      .param<FlowString>("name")
      .param<FlowString>("value")
      .param<FlowString>("delimiter", "");
  mainFunction("header.overwrite", &CoreModule::header_overwrite, FlowType::String, FlowType::String);
  mainFunction("header.remove", &CoreModule::header_remove, FlowType::String);
  mainFunction("expire", &CoreModule::expire, FlowType::Number);
  mainFunction("autoindex", &CoreModule::autoindex, FlowType::StringArray);
  mainFunction("rewrite", &CoreModule::rewrite, FlowType::String) .returnType(FlowType::Boolean);
  mainFunction("pathinfo", &CoreModule::pathinfo);

  // main: handlers
  mainHandler("docroot", &CoreModule::docroot, FlowType::String)
      .verifier(&CoreModule::verify_docroot, this);
  mainHandler("alias", &CoreModule::alias, FlowType::String, FlowType::String);
  mainHandler("staticfile", &CoreModule::staticfile);
  mainHandler("precompressed", &CoreModule::precompressed);
  mainHandler("return", &CoreModule::redirect_with_to, FlowType::Number, FlowType::String);
  mainHandler("return", &CoreModule::return_with)
      .param<FlowNumber>("status")
      .param<FlowNumber>("override", 0);
  mainHandler("echo", &CoreModule::echo, FlowType::String);
  mainHandler("blank", &CoreModule::blank);
}

CoreModule::~CoreModule() {
}

bool CoreModule::redirectOnIncompletePath(XzeroContext* cx) {
  if (!cx->file())
    return false;

  if (!cx->file()->isDirectory())
    return false;

  auto request = cx->request();
  auto response = cx->response();

  if (StringUtil::endsWith(request->path(), "/"))
    return false;

  std::string hostname = request->getHeader("X-Forwarded-Host");
  if (hostname.empty())
    hostname = request->getHeader("Host");

  std::stringstream url;
  url << (request->isSecure() ? "https://" : "http://");
  url << hostname;
  url << request->path();
  url << '/';

  if (!request->query().empty())
    url << '?' << request->query();

  response->setHeader("Location", url.str());
  response->setStatus(HttpStatus::MovedPermanently);
  response->completed();

  return true;
}

void CoreModule::mimetypes(Params& args) {
  daemon().config_->mimetypesPath = args.getString(1).str();
}

void CoreModule::mimetypes_default(Params& args) {
  daemon().config_->mimetypesDefault = args.getString(1).str();
}

void CoreModule::etag_mtime(Params& args) {
  daemon().vfs().configureETag(
      args.getBool(1),
      daemon().vfs().etagConsiderSize(),
      daemon().vfs().etagConsiderINode());
}

void CoreModule::etag_size(Params& args) {
  daemon().vfs().configureETag(
      daemon().vfs().etagConsiderMTime(),
      args.getBool(1),
      daemon().vfs().etagConsiderINode());
}

void CoreModule::etag_inode(Params& args) {
  daemon().vfs().configureETag(
      daemon().vfs().etagConsiderMTime(),
      daemon().vfs().etagConsiderSize(),
      args.getBool(1));
}

void CoreModule::fileinfo_cache_ttl(Params& args) {
  // TODO [x0d] fileinfo.cache.ttl
}

void CoreModule::server_advertise(Params& args) {
  // TODO [x0d] server.advertise
}

void CoreModule::server_tags(Params& args) {
  // TODO [x0d] server.tags
}

void CoreModule::tcp_fin_timeout(Params& args) {
  daemon().config_->tcpFinTimeout = Duration::fromSeconds(args.getInt(1));
}

void CoreModule::max_internal_redirect_count(Params& args) {
  daemon().config_->maxInternalRedirectCount = args.getInt(1);
}

void CoreModule::max_read_idle(Params& args) {
  daemon().config_->maxReadIdle = Duration::fromSeconds(args.getInt(1));
}

void CoreModule::max_write_idle(Params& args) {
  daemon().config_->maxWriteIdle = Duration::fromSeconds(args.getInt(1));
}

void CoreModule::max_keepalive_idle(Params& args) {
  daemon().config_->maxKeepAlive = Duration::fromSeconds(args.getInt(1));
}

void CoreModule::max_keepalive_requests(Params& args) {
  daemon().config_->maxKeepAliveRequests = args.getInt(1);
}

void CoreModule::max_conns(Params& args) {
  daemon().config_->maxConnections = args.getInt(1);
}

void CoreModule::max_files(Params& args) {
  setrlimit(RLIMIT_NOFILE, args.getInt(1));
}

void CoreModule::max_address_space(Params& args) {
  setrlimit(RLIMIT_AS, args.getInt(1));
}

void CoreModule::max_core(Params& args) {
  setrlimit(RLIMIT_CORE, args.getInt(1));
}

void CoreModule::tcp_cork(Params& args) {
  daemon().config_->tcpCork = args.getBool(1);
}

void CoreModule::tcp_nodelay(Params& args) {
  daemon().config_->tcpNoDelay = args.getBool(1);
}

void CoreModule::lingering(Params& args) {
  daemon().config_->lingering = Duration::fromSeconds(args.getInt(1));
}

void CoreModule::max_request_uri_size(Params& args) {
  daemon().config_->maxRequestUriLength = args.getInt(1);
}

void CoreModule::max_request_header_size(Params& args) {
  daemon().config_->maxRequestHeaderSize = args.getInt(1);
}

void CoreModule::max_request_header_count(Params& args) {
  daemon().config_->maxRequestHeaderCount = args.getInt(1);
}

void CoreModule::max_request_body_size(Params& args) {
  daemon().config_->maxRequestBodySize = args.getInt(1);
}

void CoreModule::request_header_buffer_size(Params& args) {
  daemon().config_->requestHeaderBufferSize = args.getInt(1);
}

void CoreModule::request_body_buffer_size(Params& args) {
  daemon().config_->requestBodyBufferSize = args.getInt(1);
}

void CoreModule::response_body_buffer_size(Params& args) {
  daemon().config_->responseBodyBufferSize = args.getInt(1);
}

void CoreModule::listen(Params& args) {
  IPAddress bind = args.getIPAddress(1);
  int port = args.getInt(2);
  int backlog = args.getInt(3);
  int multiAcceptCount = args.getInt(4);
  bool reuseAddr = true;
  bool deferAccept = args.getBool(5);
  bool reusePort = args.getBool(6);
  bool ssl = false;

  daemon().config_->listeners.emplace_back(ListenerConfig{
      bind, port, backlog, multiAcceptCount, reuseAddr, deferAccept, reusePort, ssl});
}

void CoreModule::ssl_listen(Params& args) {
  IPAddress bind = args.getIPAddress(1);
  int port = args.getInt(2);
  int backlog = args.getInt(3);
  int multiAcceptCount = args.getInt(4);
  bool reuseAddr = true;
  bool deferAccept = args.getBool(5);
  bool reusePort = args.getBool(6);
  bool ssl = true;

  daemon().config_->listeners.emplace_back(ListenerConfig{
      bind, port, backlog, multiAcceptCount, reuseAddr, deferAccept, reusePort, ssl});
}

void CoreModule::ssl_priorities(Params& args) {
  // TODO: sets default SSL priorities
}

void CoreModule::ssl_context(Params& args) {
  std::string keyFile = args.getString(1).str();
  std::string certFile = args.getString(2).str();
  std::string trustFile = args.getString(3).str();
  std::string priorities = args.getString(4).str();

  daemon().config_->sslContexts.emplace_back(SslContext{keyFile, certFile, trustFile, priorities});
}

void CoreModule::workers(Params& args) {
  int y = args.getInt(1);
  if (y < 0) {
    return;
  }
  size_t workerCount = static_cast<size_t>(y);

  daemon().config_->workers = workerCount;
  daemon().config_->workerAffinities.clear();

  if (workerCount == cpuCount()) {
    logDebug("x0d", "Worker count equals CPU count. Defining linear processor affinity.");
    daemon().config_->workerAffinities.resize(workerCount);
    for (size_t i = 0; i < workerCount; ++i) {
      daemon().config_->workerAffinities[i] = i;
    }
  }
}

void CoreModule::workers_affinity(Params& args) {
  const FlowIntArray& affinities = args.getIntArray(1);

  if (affinities.empty())
    RAISE(ConfigurationError, "invalid array size");

  FlowNumber numCPU = cpuCount();

  for (FlowNumber affinity: affinities)
    if (affinity >= numCPU)
      RAISE(ConfigurationError,
            "Worker's CPU affinity $0 too high. "
            "The value must be between 0 and $1.",
            numCPU);

  daemon().config_->workers = affinities.size();

  daemon().config_->workerAffinities.resize(affinities.size());
  for (size_t i = 0; i < affinities.size(); ++i)
    daemon().config_->workerAffinities[i] = (int) affinities[i];
}

void CoreModule::sys_cpu_count(XzeroContext* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cpuCount()));
}

bool CoreModule::preproc_sys_env(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder) {
  if (auto arg = dynamic_cast<ConstantString*>(call->operand(1))) {
    if (arg->get().empty()) {
      logError("x0d", "sys.env: Empty environment variable name is not allowed.");
      return false;
    }

    auto program = call->parent()->parent()->parent();  // instr -> BB ->
                                                        // handler -> program

    const char* cval = getenv(arg->get().c_str());
    ConstantString* str = program->get(cval ? cval : "");
    std::string name = builder->makeName(StringUtil::format("sys.env.$0", arg->get()));

    call->replace(new LoadInstr(str, name));
    delete call;
  }
  return true;
}

void CoreModule::sys_env(XzeroContext* cx, Params& args) {
  if (const char* value = getenv(args.getString(1).str().c_str())) {
    args.setResult(value);
  } else {
    args.setResult("");
  }
}

bool CoreModule::preproc_sys_env2(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder) {
  if (auto arg = dynamic_cast<ConstantString*>(call->operand(1))) {
    if (auto val = dynamic_cast<ConstantString*>(call->operand(2))) {
      if (arg->get().empty()) {
        logError("x0d", "sys.env: Empty environment variable name is not allowed.");
        return false;
      }

      auto program = call->parent()->parent()->parent();  // instr -> BB ->
                                                          // handler -> program

      const char* cval = getenv(arg->get().c_str());
      ConstantString* str = program->get((cval && *cval) ? cval : val->get());
      std::string name = builder->makeName(StringUtil::format("sys.env.$0", arg->get()));

      call->replace(new LoadInstr(str, name));
      delete call;
    }
  }
  return true;
}

void CoreModule::sys_env2(XzeroContext* cx, Params& args) {
  if (const char* value = getenv(args.getString(1).str().c_str())) {
    args.setResult(value);
  } else {
    args.setResult(args.getString(2).str());
  }
}

void CoreModule::sys_cwd(XzeroContext* cx, Params& args) {
  static char buf[1024];
  args.setResult(getcwd(buf, sizeof(buf)));
}

void CoreModule::sys_pid(XzeroContext* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(getpid()));
}

void CoreModule::sys_now(XzeroContext* cx, Params& args) {
  args.setResult(
      static_cast<FlowNumber>(cx->now().unixtime()));
}

void CoreModule::sys_now_str(XzeroContext* cx, Params& args) {
  static const char* timeFormat = "%a, %d %b %Y %H:%M:%S GMT";
  args.setResult(cx->now().format(timeFormat));
}

void CoreModule::sys_hostname(XzeroContext* cx, Params& args) {
  args.setResult(Application::hostname());
}

void CoreModule::sys_domainname(XzeroContext* cx, Params& args) {
  char buf[256];
  if (getdomainname(buf, sizeof(buf)) == 0) {
    args.setResult(buf);
  } else {
    logError("sys.domainname: getdomainname() failed. $0", strerror(errno));
    args.setResult("");
  }
}

void CoreModule::log_err(XzeroContext* cx, Params& args) {
  logError("x0d", "$0", args.getString(1).str());
}

void CoreModule::log_warn(XzeroContext* cx, Params& args) {
  logWarning("x0d", "$0", args.getString(1).str());
}

void CoreModule::log_notice(XzeroContext* cx, Params& args) {
  logNotice("x0d", "$0", args.getString(1).str());
}

void CoreModule::log_info(XzeroContext* cx, Params& args) {
  logInfo("x0d", "$0", args.getString(1).str());
}

void CoreModule::log_debug(XzeroContext* cx, Params& args) {
  logDebug("x0d", "$0", args.getString(1).str());
}

void CoreModule::rand(XzeroContext* cx, Params& args) {
  args.setResult(rng_.random64());
}

void CoreModule::randAB(XzeroContext* cx, Params& args) {
  FlowNumber a = args.getInt(1);
  FlowNumber b = std::max(args.getInt(2), a);
  FlowNumber y = a + (rng_.random64() % (1 + b - a));

  args.setResult(y);
}

void CoreModule::sleep(XzeroContext* cx, Params& args) {
  cx->runner()->suspend();
  cx->response()->executor()->executeAfter(
      Duration::fromSeconds(args.getInt(1)),
      std::bind(&flow::vm::Runner::resume, cx->runner()));
}

bool verifyErrorPageConfig(HttpStatus status, const std::string& uri) {
  if (!isError(status)) {
    logError("x0d", "error.page: HTTP status %d is not a client nor server error\n", status);
    return false;
  }

  if (uri.empty()) {
    logError("x0d", "error.page: Empty URIs are not allowed. Ignoring\n");
    return false;
  }

  return true;
}

void CoreModule::error_page(XzeroContext* cx, Params& args) {
  HttpStatus status = static_cast<HttpStatus>(args.getInt(1));
  std::string uri = args.getString(2).str();

  if (!verifyErrorPageConfig(status, uri))
    return;

  cx->setErrorPage(status, uri);
}

void CoreModule::error_page(Params& args) {
  HttpStatus status = static_cast<HttpStatus>(args.getInt(1));
  std::string uri = args.getString(2).str();

  if (!verifyErrorPageConfig(status, uri))
    return;

  daemon().config().errorPages[status] = uri;
}

void CoreModule::file_exists(XzeroContext* cx, Params& args) {
  auto fileinfo = daemon().vfs().getFile(args.getString(1).str(), "/");
  if (fileinfo)
    args.setResult(fileinfo->exists());
  else
    args.setResult(false);
}

void CoreModule::file_is_reg(XzeroContext* cx, Params& args) {
  auto fileinfo = daemon().vfs().getFile(args.getString(1).str(), "/");
  if (fileinfo)
    args.setResult(fileinfo->isRegular());
  else
    args.setResult(false);
}

void CoreModule::file_is_dir(XzeroContext* cx, Params& args) {
  auto fileinfo = daemon().vfs().getFile(args.getString(1).str(), "/");
  if (fileinfo)
    args.setResult(fileinfo->isDirectory());
  else
    args.setResult(false);
}

void CoreModule::file_is_exe(XzeroContext* cx, Params& args) {
  auto fileinfo = daemon().vfs().getFile(args.getString(1).str(), "/");
  if (fileinfo)
    args.setResult(fileinfo->isExecutable());
  else
    args.setResult(false);
}

bool CoreModule::verify_docroot(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder) {
  if (auto arg = dynamic_cast<ConstantString*>(call->operand(1))) {
    if (arg->get().empty()) {
      logError("x0d", "Setting empty document root is not allowed.");
      return false;
    }

    auto program = call->parent()->parent()->parent();  // instr -> BB ->
                                                        // handler -> program

    // cut off trailing slash
    size_t trailerOffset = arg->get().size() - 1;
    if (arg->get()[trailerOffset] == '/') {
      call->replaceOperand(arg,
                           program->get(arg->get().substr(0, trailerOffset)));
    }
  }

  return true;
}

bool CoreModule::docroot(XzeroContext* cx, Params& args) {
  std::string docroot = args.getString(1).str();
  docroot = FileUtil::realpath(docroot);

  cx->setDocumentRoot(docroot);
  cx->setFile(daemon().vfs().getFile(cx->request()->path(), docroot));

  return redirectOnIncompletePath(cx);
}

bool CoreModule::alias(XzeroContext* cx, Params& args) {
  // input:
  //    URI: /some/uri/path
  //    Alias '/some' => '/srv/special';
  //
  // output:
  //    docroot: /srv/special
  //    fileinfo: /srv/special/uri/path

  std::string prefix = args.getString(1).str();
  size_t prefixLength = prefix.size();
  std::string alias = args.getString(2).str();

  if (StringUtil::beginsWith(cx->request()->path(), prefix)) {
    const std::string path = alias + cx->request()->path().substr(prefixLength);
    cx->setDocumentRoot(prefix);
    cx->setFile(daemon().vfs().getFile(path, prefix));
  }

  return redirectOnIncompletePath(cx);
}

bool CoreModule::redirect_with_to(XzeroContext* cx, Params& args) {
  int status = args.getInt(1);
  FlowString location = args.getString(2);

  if (status >= 300 && status <= 308) {
    cx->response()->setStatus(static_cast<HttpStatus>(status));
    cx->response()->setHeader("Location", location.str());
  } else {
    cx->response()->setStatus(HttpStatus::InternalServerError);
    logError("x0d", 
        "Status code is out of range. %s should be between 300 and 308.",
        status);
  }
  cx->response()->completed();

  return true;
}

bool CoreModule::return_with(XzeroContext* cx, Params& args) {
  HttpStatus status = static_cast<HttpStatus>(args.getInt(1));
  HttpStatus overrideStatus = static_cast<HttpStatus>(args.getInt(2));

  // internal redirects rewind the instruction pointer, starting from
  // the entry point again, so the handler than should not return success (true).
  bool internalRedirect = false;
  cx->sendErrorPage(status, &internalRedirect);
  return internalRedirect == false;
}

bool CoreModule::echo(XzeroContext* cx, Params& args) {
  auto content = args.getString(1);

  if (!cx->response()->status())
    cx->response()->setStatus(HttpStatus::Ok);

  cx->response()->write(content);
  cx->response()->write("\n");
  cx->response()->completed();

  return true;
}

bool CoreModule::blank(XzeroContext* cx, Params& args) {
  cx->response()->setStatus(HttpStatus::Ok);
  cx->response()->completed();
  return true;
}

bool CoreModule::staticfile(XzeroContext* cx, Params& args) {
  if (!cx->verifyDirectoryDepth())
    return true;

  HttpStatus status = daemon().fileHandler().handle(cx->request(),
                                                    cx->response(),
                                                    cx->file());
  if (status == HttpStatus::NotFound) {
    return false;
  } else if (!isError(status)) {
    return true;
  } else {
    bool internalRedirect = false;
    cx->sendErrorPage(status, &internalRedirect);
    return internalRedirect == false;
  }
}

bool CoreModule::precompressed(XzeroContext* cx, Params& args) {
  if (!cx->verifyDirectoryDepth())
    return true;

  if (!cx->file())
    return false;

  if (!cx->file()->exists())
    return false;

  if (!cx->file()->isRegular())
    return false;

  const std::string& r = cx->request()->getHeader("Accept-Encoding");
  if (!r.empty()) {
    auto items = Tokenizer<BufferRef>::tokenize(BufferRef(r), ", ");

    static const struct {
      const char* id;
      const char* fileExtension;
    } encodings[] = {
      {"gzip", ".gz"},
      {"bzip2", ".bz2"},
    };

    for (const auto& encoding: encodings) {
      if (std::find(items.begin(), items.end(), encoding.id) == items.end())
        continue;

      auto pc = daemon().vfs().getFile(
          cx->file()->path() + encoding.fileExtension, "/");

      if (pc->exists() && pc->isRegular() &&
          pc->mtime() == cx->file()->mtime()) {
        // XXX we assign pc to request's fileinfo here, so we preserve a
        // reference until the
        // file was fully transmitted to the client.
        // otherwise the pc's reference count can go down to zero at the end
        // of this scope
        // without having the file fully sent out yet.
        // FIXME: sendfile() should accept HttpFileView instead.
        cx->setFile(pc);

        cx->response()->setHeader("Content-Encoding", encoding.id);
        HttpStatus status = daemon().fileHandler().handle(cx->request(),
                                                          cx->response(),
                                                          cx->file());
        if (status == HttpStatus::NotFound) {
          return false;
        } else if (!isError(status)) {
          return true;
        } else {
          bool internalRedirect = false;
          cx->sendErrorPage(status, &internalRedirect);
          return internalRedirect == false;
        }
      }
    }
  }

  return false;
}

void CoreModule::autoindex(XzeroContext* cx, Params& args) {
  if (cx->documentRoot().empty()) {
    logError("x0d", "autoindex: No document root set yet. Skipping.");
    // error: must have a document-root set first.
    return;
  }

  if (!cx->file())
    // something went wrong, just be sure we SEGFAULT here
    RAISE(InternalError, "BUG: cx->file is NULL");

  if (!cx->file()->isDirectory())
    return;

  const FlowStringArray& indexfiles = args.getStringArray(1);
  for (size_t i = 0, e = indexfiles.size(); i != e; ++i) {
    if (matchIndex(cx, indexfiles[i])) {
      return;
    }
  }
}

bool CoreModule::matchIndex(XzeroContext* cx, const xzero::BufferRef& arg) {
  std::string ipath = FileUtil::joinPaths(cx->file()->path(), arg.str());

  if (auto fi = daemon().vfs().getFile(ipath, cx->documentRoot())) {
    if (fi->isRegular()) {
      cx->setFile(fi);
      return true;
    }
  }

  return false;
}

void CoreModule::rewrite(XzeroContext* cx, Params& args) {
  auto file = daemon().vfs().getFile(args.getString(1).str(), cx->documentRoot());
  cx->setFile(file);
  args.setResult(file ? file->exists() : false);
}

void CoreModule::pathinfo(XzeroContext* cx, Params& args) {
  if (!cx->file()) {
    logError("x0d",
        "pathinfo: no file information available. "
        "Please set document root first.");
    return;
  }

  // split "/the/tail" from "/path/to/script.php/the/tail"

  std::shared_ptr<File> file = cx->file();
  std::string fullname = file->path();
  size_t origpos = fullname.size() - 1;
  size_t pos = origpos;

  for (;;) {
    if (file->exists()) {
      if (pos != origpos) {
        off_t ofs = cx->request()->path().size() - (origpos - pos + 1);
        std::string pi = cx->request()->path().substr(ofs);
        cx->setPathInfo(pi);
      }
      break;
    }

    if (file->errorCode() == ENOTDIR) {
      pos = file->path().rfind('/', pos - 1);
      file = daemon().vfs().getFile(file->path().substr(0, pos), "/");
      cx->setFile(file);
    } else {
      break;
    }
  }
}

void CoreModule::header_add(XzeroContext* cx, Params& args) {
  std::string name = args.getString(1).str();
  std::string value = args.getString(2).str();

  cx->response()->onPostProcess([cx, name, value]() {
    cx->response()->addHeader(name, value);
  });
}

void CoreModule::header_append(XzeroContext* cx, Params& args) {
  std::string name = args.getString(1).str();
  std::string value = args.getString(2).str();
  std::string delim = args.getString(3).str();

  cx->response()->onPostProcess([cx, name, value, delim]() {
    cx->response()->appendHeader(name, value, delim);
  });
}

void CoreModule::header_overwrite(XzeroContext* cx, Params& args) {
  std::string name = args.getString(1).str();
  std::string value = args.getString(2).str();

  cx->response()->onPostProcess([cx, name, value]() {
    cx->response()->setHeader(name, value);
  });
}

void CoreModule::header_remove(XzeroContext* cx, Params& args) {
  std::string name = args.getString(1).str();

  cx->response()->onPostProcess([cx, name]() {
    cx->response()->removeHeader(name);
  });
}

void CoreModule::expire(XzeroContext* cx, Params& args) {
  time_t now = cx->now().unixtime();
  time_t mtime = cx->file() ? cx->file()->mtime() : now;
  time_t value = args.getInt(1);

  // passed a timespan
  if (value < mtime)
    value = value + now;

  // (mtime+span) points to past?
  if (value < now)
    value = now;

  static const char* timeFormat = "%a, %d %b %Y %H:%M:%S GMT";
  cx->response()->setHeader("Expires", UnixTime(value).format(timeFormat));

  cx->response()->setHeader("Cache-Control",
      StringUtil::format("max-age=$0", value - now));
}

void CoreModule::req_method(XzeroContext* cx, Params& args) {
  args.setResult(cx->request()->unparsedMethod());
}

void CoreModule::req_url(XzeroContext* cx, Params& args) {
  args.setResult(cx->request()->unparsedUri());
}

void CoreModule::req_path(XzeroContext* cx, Params& args) {
  args.setResult(cx->request()->path());
}

void CoreModule::req_query(XzeroContext* cx, Params& args) {
  args.setResult(cx->request()->query());
}

void CoreModule::req_header(XzeroContext* cx, Params& args) {
  args.setResult(cx->request()->getHeader(args.getString(1).str()));
}

void CoreModule::req_cookie(XzeroContext* cx, Params& args) {
  std::string cookie = cx->request()->getHeader("Cookie");
  if (!cookie.empty()) {
    auto wanted = args.getString(1);
    static const std::string sld("; \t");
    Tokenizer<BufferRef> st1(BufferRef(cookie), sld);
    BufferRef kv;

    while (!(kv = st1.nextToken()).empty()) {
      static const std::string s2d("= \t");
      Tokenizer<BufferRef> st2(kv, s2d);
      BufferRef key(st2.nextToken());
      BufferRef value(st2.nextToken());
      // printf("parsed cookie[%s] = '%s'\n", key.c_str(), value.c_str());
      if (key == wanted) {
        args.setResult(value);
        return;
      }
      // cookies_[key] = value;
    }
  }
  args.setResult("");
}

void CoreModule::req_host(XzeroContext* cx, Params& args) {
  args.setResult(cx->request()->host());
}

void CoreModule::req_pathinfo(XzeroContext* cx, Params& args) {
  args.setResult(cx->pathInfo());
}

void CoreModule::req_is_secure(XzeroContext* cx, Params& args) {
  args.setResult(cx->request()->isSecure());
}

void CoreModule::req_scheme(XzeroContext* cx, Params& args) {
  args.setResult(cx->request()->isSecure() ? "https" : "http");
}

void CoreModule::req_status_code(XzeroContext* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->response()->status()));
}

void CoreModule::conn_remote_ip(XzeroContext* cx, Params& args) {
  args.setResult(&(cx->remoteIP()));
}

void CoreModule::conn_remote_port(XzeroContext* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->remotePort()));
}

void CoreModule::conn_local_ip(XzeroContext* cx, Params& args) {
  args.setResult(&(cx->localIP()));
}

void CoreModule::conn_local_port(XzeroContext* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->localPort()));
}

void CoreModule::phys_path(XzeroContext* cx, Params& args) {
  static const std::string none;
  args.setResult(cx->file() ? cx->file()->path() : none);
}

void CoreModule::phys_exists(XzeroContext* cx, Params& args) {
  args.setResult(cx->file() ? cx->file()->exists() : false);
}

void CoreModule::phys_is_reg(XzeroContext* cx, Params& args) {
  args.setResult(cx->file() ? cx->file()->isRegular() : false);
}

void CoreModule::phys_is_dir(XzeroContext* cx, Params& args) {
  args.setResult(cx->file() ? cx->file()->isDirectory() : false);
}

void CoreModule::phys_is_exe(XzeroContext* cx, Params& args) {
  args.setResult(cx->file() ? cx->file()->isExecutable() : false);
}

void CoreModule::phys_mtime(XzeroContext* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->file() ? cx->file()->mtime() : 0));
}

void CoreModule::phys_size(XzeroContext* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->file() ? cx->file()->size() : 0));
}

void CoreModule::phys_etag(XzeroContext* cx, Params& args) {
  static const std::string none;
  args.setResult(cx->file() ? cx->file()->etag() : none);
}

void CoreModule::phys_mimetype(XzeroContext* cx, Params& args) {
  static const std::string none;
  args.setResult(cx->file() ? cx->file()->mimetype() : none);
}

void CoreModule::regex_group(XzeroContext* cx, Params& args) {
  FlowNumber position = args.getInt(1);

  if (const RegExp::Result* rr = cx->runner()->regexpContext()->regexMatch()) {
    if (position >= 0 && position < static_cast<FlowNumber>(rr->size())) {
      const auto& match = rr->at(position);
      FlowString result(match.first, match.second);
      args.setResult(args.caller()->newString(match.first, match.second));
    } else {
      // match index out of bounds
      args.setResult("");
    }
  } else {
    // no regex match executed
    args.setResult("");
  }
}

void CoreModule::req_accept_language(XzeroContext* cx, Params& args) {
  const FlowStringArray& supportedLanguages = args.getStringArray(1);
  std::string acceptLanguage = cx->request()->getHeader("Accept-Language");

  if (acceptLanguage.empty()) {
    args.setResult(supportedLanguages[0]);
    return;
  }

  const char* i = acceptLanguage.data();
  const char* e = i + acceptLanguage.size();

  auto skipSpaces = [&]() -> bool {
    while (i != e && std::isspace(*i)) {
      ++i;
    }
    return i != e;
  };

  auto parseToken = [&]() -> BufferRef {
    const char* beg = i;
    while (i != e && (std::isalnum(*i) || *i == '-' || *i == '_')) {
      ++i;
    }
    return BufferRef(beg, i - beg);
  };

  auto isSupported = [&](const BufferRef& language) -> bool {
    for (const auto& lang : supportedLanguages) {
      if (iequals(lang, language)) {
        return true;
      }
    }
    return false;
  };

  // AcceptLanguage   ::= Language (',' Language)*
  // Language         ::= TOKEN [';' Attribs]
  while (i != e) {
    if (!skipSpaces()) {
      break;
    }

    auto token = parseToken();

    if (isSupported(token)) {
      args.setResult(args.caller()->newString(token.data(), token.size()));
      return;
    }

    // consume until delimiter
    while (i != e && *i != ',') {
      ++i;
    }

    // consume delimiter
    while (i != e && (*i == ',' || std::isspace(*i))) {
      ++i;
    }
  }

  args.setResult(supportedLanguages[0]);
}

bool CoreModule::verify_req_accept_language(xzero::flow::Instr* call, xzero::flow::IRBuilder* builder) {
  auto arg = dynamic_cast<ConstantArray*>(call->operand(1));
  assert(arg != nullptr);

  // empty-arrays aren't currently supported, but write the test in case I
  // changed my mind on the other side. ;)
  if (arg->get().size() == 0) {
    logError("x0d",
        "req.accept_language() requires a non-empty array argument.");
    return false;
  }

  return true;
}

} // namespace x0d
