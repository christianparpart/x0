// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include "core.h"
#include <x0d/Module.h>
#include <x0d/Context.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/io/FileUtil.h>
#include <xzero/Application.h>
#include <xzero/WallClock.h>
#include <xzero/StringUtil.h>
#include <xzero/RuntimeError.h>
#include <xzero/Tokenizer.h>
#include <xzero/Buffer.h>
#include <xzero/sysconfig.h>
#include <sstream>
#include <cmath>
#include <cctype>

#if defined(XZERO_OS_UNIX)
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#endif

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

using namespace xzero;
using namespace xzero::http;
using namespace flow;

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

#if defined(HAVE_SETRLIMIT)
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
    logWarning("Failed to retrieve current resource limit on {} ({}).",
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
    logWarning("Failed to set resource limit on {} from {} to {}.",
               rc2str(resource), hlast, hvalue);

    return 0;
  }

  logTrace("Set resource limit on {} from {} to {}.",
           rc2str(resource), hlast, hvalue);

  return value;
}
#endif

size_t CoreModule::cpuCount() {
  static size_t numCPU_ = Application::processorCount();
  return numCPU_;
}

CoreModule::CoreModule(Daemon* d)
    : Module(d, "core"),
      rng_() {

  // setup functions
  setupFunction("listen", &CoreModule::listen)
      .param<flow::util::IPAddress>("address", flow::util::IPAddress("0.0.0.0"))
      .param<int>("port")
      .param<int>("backlog", 0) // <= 0 means, it'll default to system-default
      .param<int>("multi_accept", 1)
      .param<bool>("defer_accept", false)
      .param<bool>("reuse_port", false);

  setupFunction("ssl.listen", &CoreModule::ssl_listen)
      .param<flow::util::IPAddress>("address", flow::util::IPAddress("0.0.0.0"))
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

  setupFunction("ssl.priorities", &CoreModule::ssl_priorities, flow::LiteralType::String);

  // setup: properties (write-only)
  setupFunction("workers", &CoreModule::workers, LiteralType::Number);
  setupFunction("workers", &CoreModule::workers_affinity, LiteralType::IntArray);
  setupFunction("mimetypes", &CoreModule::mimetypes, LiteralType::String);
  setupFunction("mimetypes.default", &CoreModule::mimetypes_default, LiteralType::String);
  setupFunction("etag.mtime", &CoreModule::etag_mtime, LiteralType::Boolean);
  setupFunction("etag.size", &CoreModule::etag_size, LiteralType::Boolean);
  setupFunction("etag.inode", &CoreModule::etag_inode, LiteralType::Boolean);
  setupFunction("fileinfo.ttl", &CoreModule::fileinfo_cache_ttl, LiteralType::Number);
  setupFunction("server.advertise", &CoreModule::server_advertise, LiteralType::Boolean);
  setupFunction("server.tags", &CoreModule::server_tags, LiteralType::StringArray, LiteralType::String);
  setupFunction("tcp_fin_timeout", &CoreModule::tcp_fin_timeout, LiteralType::Number);
  setupFunction("max_internal_redirect_count", &CoreModule::max_internal_redirect_count, LiteralType::Number);
  setupFunction("max_read_idle", &CoreModule::max_read_idle, LiteralType::Number);
  setupFunction("max_write_idle", &CoreModule::max_write_idle, LiteralType::Number);
  setupFunction("max_keepalive_idle", &CoreModule::max_keepalive_idle, LiteralType::Number);
  setupFunction("max_keepalive_requests", &CoreModule::max_keepalive_requests, LiteralType::Number);
  setupFunction("max_connections", &CoreModule::max_conns, LiteralType::Number);
  setupFunction("max_files", &CoreModule::max_files, LiteralType::Number);
  setupFunction("max_address_space", &CoreModule::max_address_space, LiteralType::Number);
  setupFunction("max_core_size", &CoreModule::max_core, LiteralType::Number);
  setupFunction("tcp_cork", &CoreModule::tcp_cork, LiteralType::Boolean);
  setupFunction("tcp_nodelay", &CoreModule::tcp_nodelay, LiteralType::Boolean);
  setupFunction("lingering", &CoreModule::lingering, LiteralType::Number);
  setupFunction("max_request_uri_size", &CoreModule::max_request_uri_size, LiteralType::Number);
  setupFunction("max_request_header_size", &CoreModule::max_request_header_size, LiteralType::Number);
  setupFunction("max_request_header_count", &CoreModule::max_request_header_count, LiteralType::Number);
  setupFunction("max_request_body_size", &CoreModule::max_request_body_size, LiteralType::Number);
  setupFunction("request_header_buffer_size", &CoreModule::request_header_buffer_size, LiteralType::Number);
  setupFunction("request_body_buffer_size", &CoreModule::request_body_buffer_size, LiteralType::Number);
  setupFunction("response_body_buffer_size", &CoreModule::response_body_buffer_size, LiteralType::Number);

  // TODO setup error-documents

  // shared properties (read-only)
  sharedFunction("sys.cpu_count", &CoreModule::sys_cpu_count)
      .setReadOnly()
      .returnType(LiteralType::Number);
  sharedFunction("sys.env", &CoreModule::sys_env, LiteralType::String)
      .setReadOnly()
      .returnType(LiteralType::String)
      .verifier(&CoreModule::preproc_sys_env, this);
  sharedFunction("sys.env", &CoreModule::sys_env2, LiteralType::String, LiteralType::String)
      .setReadOnly()
      .returnType(LiteralType::String)
      .verifier(&CoreModule::preproc_sys_env2, this);
  sharedFunction("sys.cwd", &CoreModule::sys_cwd)
      .setReadOnly()
      .returnType(LiteralType::String);
  sharedFunction("sys.pid", &CoreModule::sys_pid)
      .setReadOnly()
      .returnType(LiteralType::Number);
  sharedFunction("sys.now", &CoreModule::sys_now)
      .setReadOnly()
      .returnType(LiteralType::Number);
  sharedFunction("sys.now_str", &CoreModule::sys_now_str)
      .setReadOnly()
      .returnType(LiteralType::String);
  sharedFunction("sys.hostname", &CoreModule::sys_hostname)
      .setReadOnly()
      .returnType(LiteralType::String);
  sharedFunction("sys.domainname", &CoreModule::sys_domainname)
      .setReadOnly()
      .returnType(LiteralType::String);
  sharedFunction("sys.max_conn", &CoreModule::sys_max_conn)
      .setReadOnly()
      .returnType(LiteralType::Number);

  // shared functions
  sharedFunction("error.page", &CoreModule::error_page_,
                               &CoreModule::error_page)
      .param<FlowNumber>("status")
      .param<FlowString>("uri");
  sharedFunction("file.exists", &CoreModule::file_exists, LiteralType::String)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  sharedFunction("file.is_reg", &CoreModule::file_is_reg, LiteralType::String)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  sharedFunction("file.is_dir", &CoreModule::file_is_dir, LiteralType::String)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  sharedFunction("file.is_exe", &CoreModule::file_is_exe, LiteralType::String)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  sharedFunction("log.err", &CoreModule::log_err, LiteralType::String);
  sharedFunction("log.warn", &CoreModule::log_warn, LiteralType::String);
  sharedFunction("log.notice", &CoreModule::log_notice, LiteralType::String);
  sharedFunction("log", &CoreModule::log_info, LiteralType::String);
  sharedFunction("log.info", &CoreModule::log_info, LiteralType::String);
  sharedFunction("log.debug", &CoreModule::log_debug, LiteralType::String);
  sharedFunction("sleep", &CoreModule::sleep, LiteralType::Number);
  sharedFunction("rand", &CoreModule::rand)
      .returnType(LiteralType::Number);
  sharedFunction("rand", &CoreModule::randAB, LiteralType::Number, LiteralType::Number)
      .returnType(LiteralType::Number);

  // main: read-only attributes
  mainFunction("req.method", &CoreModule::req_method)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.url", &CoreModule::req_url)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.path", &CoreModule::req_path)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.query", &CoreModule::req_query)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.header", &CoreModule::req_header, LiteralType::String)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.cookie", &CoreModule::req_cookie, LiteralType::String)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.host", &CoreModule::req_host)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.pathinfo", &CoreModule::req_pathinfo)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.is_secure", &CoreModule::req_is_secure)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  mainFunction("req.scheme", &CoreModule::req_scheme)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("req.status", &CoreModule::req_status_code)
      .setReadOnly()
      .returnType(LiteralType::Number);
  mainFunction("req.remoteip", &CoreModule::conn_remote_ip)
      .setReadOnly()
      .returnType(LiteralType::IPAddress);
  mainFunction("req.remoteport", &CoreModule::conn_remote_port)
      .setReadOnly()
      .returnType(LiteralType::Number);
  mainFunction("req.localip", &CoreModule::conn_local_ip)
      .setReadOnly()
      .returnType(LiteralType::IPAddress);
  mainFunction("req.localport", &CoreModule::conn_local_port)
      .setReadOnly()
      .returnType(LiteralType::Number);
  mainFunction("phys.path", &CoreModule::phys_path)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("phys.exists", &CoreModule::phys_exists)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  mainFunction("phys.is_reg", &CoreModule::phys_is_reg)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  mainFunction("phys.is_dir", &CoreModule::phys_is_dir)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  mainFunction("phys.is_exe", &CoreModule::phys_is_exe)
      .setReadOnly()
      .returnType(LiteralType::Boolean);
  mainFunction("phys.mtime", &CoreModule::phys_mtime)
      .setReadOnly()
      .returnType(LiteralType::Number);
  mainFunction("phys.size", &CoreModule::phys_size)
      .setReadOnly()
      .returnType(LiteralType::Number);
  mainFunction("phys.etag", &CoreModule::phys_etag)
      .setReadOnly()
      .returnType(LiteralType::String);
  mainFunction("phys.mimetype", &CoreModule::phys_mimetype)
      .setReadOnly()
      .returnType(LiteralType::String);

  // main: getter functions
  mainFunction("req.accept_language", &CoreModule::req_accept_language, LiteralType::StringArray)
      .setReadOnly()
      .returnType(LiteralType::String)
      .verifier(&CoreModule::verify_req_accept_language, this);
  mainFunction("regex.group", &CoreModule::regex_group, LiteralType::Number)
      .setReadOnly()
      .returnType(LiteralType::String);

  // main: manipulation functions
  mainFunction("header.add", &CoreModule::header_add, LiteralType::String, LiteralType::String);
  mainFunction("header.append", &CoreModule::header_append)
      .param<FlowString>("name")
      .param<FlowString>("value")
      .param<FlowString>("delimiter", "");
  mainFunction("header.overwrite", &CoreModule::header_overwrite, LiteralType::String, LiteralType::String);
  mainFunction("header.remove", &CoreModule::header_remove, LiteralType::String);
  mainFunction("expire", &CoreModule::expire, LiteralType::Number);
  mainFunction("autoindex", &CoreModule::autoindex, LiteralType::StringArray);
  mainFunction("rewrite", &CoreModule::rewrite, LiteralType::String) .returnType(LiteralType::Boolean);
  mainFunction("pathinfo", &CoreModule::pathinfo);

  // main: handlers
  mainHandler("docroot", &CoreModule::docroot, LiteralType::String)
      .verifier(&CoreModule::verify_docroot, this);
  mainHandler("alias", &CoreModule::alias, LiteralType::String, LiteralType::String);
  mainHandler("staticfile", &CoreModule::staticfile);
  mainHandler("precompressed", &CoreModule::precompressed);
  mainHandler("return", &CoreModule::redirect_with_to)
      .setNoReturn()
      .param<FlowNumber>("status")
      .param<FlowString>("to");
  mainHandler("return", &CoreModule::return_with)
      .setNoReturn()
      .param<FlowNumber>("status")
      .param<FlowNumber>("override", 0);
  mainHandler("echo", &CoreModule::echo, LiteralType::String);
  mainHandler("blank", &CoreModule::blank);
}

CoreModule::~CoreModule() {
}

bool CoreModule::redirectOnIncompletePath(Context* cx) {
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
  daemon().config_->mimetypesPath = args.getString(1);
}

void CoreModule::mimetypes_default(Params& args) {
  daemon().config_->mimetypesDefault = args.getString(1);
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
#if defined(HAVE_SETRLIMIT)
  setrlimit(RLIMIT_NOFILE, args.getInt(1));
#endif
}

void CoreModule::max_address_space(Params& args) {
#if defined(HAVE_SETRLIMIT)
  setrlimit(RLIMIT_AS, args.getInt(1));
#endif
}

void CoreModule::max_core(Params& args) {
#if defined(HAVE_SETRLIMIT)
  setrlimit(RLIMIT_CORE, args.getInt(1));
#endif
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
  IPAddress bind{static_cast<int>(args.getIPAddress(1).family()), args.getIPAddress(1).data()};
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
  IPAddress bind{static_cast<int>(args.getIPAddress(1).family()), args.getIPAddress(1).data()};
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
  std::string keyFile = args.getString(1);
  std::string certFile = args.getString(2);
  std::string trustFile = args.getString(3);
  std::string priorities = args.getString(4);

  daemon().config_->sslContexts.emplace_back(SslContext{certFile, keyFile, trustFile, priorities});
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
    logDebug("Worker count equals CPU count. Defining linear processor affinity.");
    daemon().config_->workerAffinities.resize(workerCount);
    for (size_t i = 0; i < workerCount; ++i) {
      daemon().config_->workerAffinities[i] = i;
    }
  }
}

void CoreModule::workers_affinity(Params& args) {
  const FlowIntArray& affinities = args.getIntArray(1);

  if (affinities.empty())
    throw ConfigurationError{"invalid array size"};

  FlowNumber numCPU = cpuCount();

  for (FlowNumber affinity: affinities)
    if (affinity >= numCPU)
      throw ConfigurationError{fmt::format(
            "Worker's CPU affinity {} too high. "
            "The value must be between 0 and {}.",
            affinity, numCPU)};

  daemon().config_->workers = affinities.size();

  daemon().config_->workerAffinities.resize(affinities.size());
  for (size_t i = 0; i < affinities.size(); ++i)
    daemon().config_->workerAffinities[i] = (int) affinities[i];
}

void CoreModule::sys_cpu_count(Context* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cpuCount()));
}

bool CoreModule::preproc_sys_env(flow::Instr* call, flow::IRBuilder* builder) {
  if (auto arg = dynamic_cast<ConstantString*>(call->operand(1))) {
    if (arg->get().empty()) {
      logError("sys.env: Empty environment variable name is not allowed.");
      return false;
    }

    auto program = call->getBasicBlock()->getHandler()->getProgram();

    const char* cval = getenv(arg->get().c_str());
    ConstantString* str = program->get(cval ? cval : "");
    std::string name = builder->makeName(fmt::format("sys.env.{}", arg->get()));

    call->replace(std::make_unique<LoadInstr>(str, name));
  }
  return true;
}

void CoreModule::sys_env(Context* cx, Params& args) {
  if (const char* value = getenv(args.getString(1).c_str())) {
    args.setResult(value);
  } else {
    args.setResult("");
  }
}

bool CoreModule::preproc_sys_env2(flow::Instr* call, flow::IRBuilder* builder) {
  if (auto arg = dynamic_cast<ConstantString*>(call->operand(1))) {
    if (auto val = dynamic_cast<ConstantString*>(call->operand(2))) {
      if (arg->get().empty()) {
        logError("sys.env: Empty environment variable name is not allowed.");
        return false;
      }

      auto program = call->getBasicBlock()->getHandler()->getProgram();

      const char* cval = getenv(arg->get().c_str());
      ConstantString* str = program->get((cval && *cval) ? cval : val->get());
      std::string name = builder->makeName(fmt::format("sys.env.{}", arg->get()));

      call->replace(std::make_unique<LoadInstr>(str, name));
    }
  }
  return true;
}

void CoreModule::sys_env2(Context* cx, Params& args) {
  if (const char* value = getenv(args.getString(1).c_str())) {
    args.setResult(value);
  } else {
    args.setResult(args.getString(2));
  }
}

void CoreModule::sys_cwd(Context* cx, Params& args) {
  args.setResult(fs::current_path().string());
}

void CoreModule::sys_pid(Context* cx, Params& args) {
#if defined(XZERO_OS_WINDOWS)
  args.setResult(static_cast<FlowNumber>(GetProcessId(GetCurrentProcess())));
#else
  args.setResult(static_cast<FlowNumber>(getpid()));
#endif
}

void CoreModule::sys_now(Context* cx, Params& args) {
  args.setResult(
      static_cast<FlowNumber>(cx->now().unixtime()));
}

void CoreModule::sys_now_str(Context* cx, Params& args) {
  static const char* timeFormat = "%a, %d %b %Y %T GMT";
  args.setResult(cx->now().format(timeFormat));
}

void CoreModule::sys_hostname(Context* cx, Params& args) {
  args.setResult(Application::hostname());
}

void CoreModule::sys_domainname(Context* cx, Params& args) {
#if defined(XZERO_OS_WINDOWS)
  cx->logError("sys.domainname: Not supported.");
  args.setResult("");
#else
  char buf[256];
  if (getdomainname(buf, sizeof(buf)) == 0) {
    args.setResult(buf);
  } else {
    cx->logError("sys.domainname: getdomainname() failed. {}", strerror(errno));
    args.setResult("");
  }
#endif
}

void CoreModule::sys_max_conn(Context* cx, Params& args) {
  FlowNumber somaxconn = SOMAXCONN;

#if defined(XZERO_OS_LINUX)
  somaxconn = FileUtil::read("/proc/sys/net/core/somaxconn").toInt();
#endif

  args.setResult(somaxconn);
}

void CoreModule::log_err(Context* cx, Params& args) {
  if (cx)
    cx->logError("{}", args.getString(1));
  else
    logError("{}", args.getString(1));
}

void CoreModule::log_warn(Context* cx, Params& args) {
  if (cx)
    cx->logWarning("{}", args.getString(1));
  else
    logWarning("{}", args.getString(1));
}

void CoreModule::log_notice(Context* cx, Params& args) {
  if (cx)
    cx->logNotice("{}", args.getString(1));
  else
    logNotice("{}", args.getString(1));
}

void CoreModule::log_info(Context* cx, Params& args) {
  if (cx)
    cx->logInfo("{}", args.getString(1));
  else
    logInfo("{}", args.getString(1));
}

void CoreModule::log_debug(Context* cx, Params& args) {
  if (cx)
    cx->logDebug(args.getString(1));
  else
    logDebug(args.getString(1));
}

void CoreModule::rand(Context* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(rng_.random64()));
}

void CoreModule::randAB(Context* cx, Params& args) {
  FlowNumber a = args.getInt(1);
  FlowNumber b = std::max(args.getInt(2), a);
  FlowNumber y = a + (rng_.random64() % (1 + b - a));

  args.setResult(y);
}

void CoreModule::sleep(Context* cx, Params& args) {
  cx->runner()->suspend();
  cx->response()->executor()->executeAfter(
      Duration::fromSeconds(args.getInt(1)),
      std::bind(&flow::Runner::resume, cx->runner()));
}

bool verifyErrorPageConfig(HttpStatus status, const std::string& uri) {
  if (!isError(status)) {
    logError("error.page: HTTP status %d is not a client nor server error\n", status);
    return false;
  }

  if (uri.empty()) {
    logError("error.page: Empty URIs are not allowed. Ignoring\n");
    return false;
  }

  return true;
}

void CoreModule::error_page(Context* cx, Params& args) {
  HttpStatus status = static_cast<HttpStatus>(args.getInt(1));
  std::string uri = args.getString(2);

  if (!verifyErrorPageConfig(status, uri))
    return;

  cx->setErrorPage(status, uri);
}

void CoreModule::error_page_(Params& args) {
  HttpStatus status = static_cast<HttpStatus>(args.getInt(1));
  std::string uri = args.getString(2);

  if (!verifyErrorPageConfig(status, uri))
    return;

  daemon().config().errorPages[status] = uri;
}

void CoreModule::file_exists(Context* cx, Params& args) {
  auto fileinfo = daemon().vfs().getFile(args.getString(1));
  if (fileinfo)
    args.setResult(fileinfo->exists());
  else
    args.setResult(false);
}

void CoreModule::file_is_reg(Context* cx, Params& args) {
  auto fileinfo = daemon().vfs().getFile(args.getString(1));
  if (fileinfo)
    args.setResult(fileinfo->isRegular());
  else
    args.setResult(false);
}

void CoreModule::file_is_dir(Context* cx, Params& args) {
  auto fileinfo = daemon().vfs().getFile(args.getString(1));
  if (fileinfo)
    args.setResult(fileinfo->isDirectory());
  else
    args.setResult(false);
}

void CoreModule::file_is_exe(Context* cx, Params& args) {
  auto fileinfo = daemon().vfs().getFile(args.getString(1));
  if (fileinfo)
    args.setResult(fileinfo->isExecutable());
  else
    args.setResult(false);
}

bool CoreModule::verify_docroot(flow::Instr* call, flow::IRBuilder* builder) {
  if (auto arg = dynamic_cast<ConstantString*>(call->operand(1))) {
    if (arg->get().empty()) {
      logError("Setting empty document root is not allowed.");
      return false;
    }

    auto program = call->getBasicBlock()->getHandler()->getProgram();

    // cut off trailing slash
    size_t trailerOffset = arg->get().size() - 1;
    if (arg->get()[trailerOffset] == '/') {
      call->replaceOperand(arg,
                           program->get(arg->get().substr(0, trailerOffset)));
    }
  }

  return true;
}

bool CoreModule::docroot(Context* cx, Params& args) {
  std::string path = args.getString(1);
  Result<std::string> realpath = FileUtil::realpath(path);
  if (realpath.isFailure()) {
    cx->logError("docroot: Could not find docroot '{}'. ({}) {}",
        path,
        realpath.error().category().name(),
        realpath.error().message());
    return cx->sendErrorPage(HttpStatus::InternalServerError);
  }
  std::string filepath = FileUtil::joinPaths(*realpath, cx->request()->path());

  cx->setDocumentRoot(*realpath);
  cx->setFile(daemon().vfs().getFile(filepath));

  return redirectOnIncompletePath(cx);
}

bool CoreModule::alias(Context* cx, Params& args) {
  // input:
  //    URI: /some/uri/path
  //    Alias '/some' => '/srv/special';
  //
  // output:
  //    docroot: /srv/special
  //    fileinfo: /srv/special/uri/path

  std::string prefix = args.getString(1);
  size_t prefixLength = prefix.size();
  std::string alias = args.getString(2);

  if (StringUtil::beginsWith(cx->request()->path(), prefix)) {
    const std::string path = alias + cx->request()->path().substr(prefixLength);
    const std::string filepath = FileUtil::joinPaths(prefix, path);
    cx->setDocumentRoot(prefix);
    cx->setFile(daemon().vfs().getFile(filepath));
  }

  return redirectOnIncompletePath(cx);
}

bool CoreModule::redirect_with_to(Context* cx, Params& args) {
  if (cx->tryServeTraceOrigin())
    return true;

  int status = args.getInt(1);
  FlowString location = args.getString(2);

  if (status >= 300 && status <= 308) {
    cx->response()->setStatus(static_cast<HttpStatus>(status));
    cx->response()->setHeader("Location", location);
  } else {
    cx->response()->setStatus(HttpStatus::InternalServerError);
    cx->logError("Status code is out of range. %s should be between 300 and 308.", status);
  }
  cx->response()->completed();

  return true;
}

bool CoreModule::return_with(Context* cx, Params& args) {
  if (cx->tryServeTraceOrigin())
    return true;

  HttpStatus status = static_cast<HttpStatus>(args.getInt(1));
  HttpStatus overrideStatus = static_cast<HttpStatus>(args.getInt(2));

  // internal redirects rewind the instruction pointer, starting from
  // the entry point again, so the handler than should not return success (true).
  return cx->sendErrorPage(status);
}

bool CoreModule::echo(Context* cx, Params& args) {
  if (cx->tryServeTraceOrigin())
    return true;

  auto content = args.getString(1);

  if (!cx->response()->status())
    cx->response()->setStatus(HttpStatus::Ok);

  cx->response()->write(content);
  cx->response()->write("\n");
  cx->response()->completed();

  return true;
}

bool CoreModule::blank(Context* cx, Params& args) {
  if (cx->tryServeTraceOrigin())
    return true;

  cx->response()->setStatus(HttpStatus::Ok);
  cx->response()->completed();
  return true;
}

bool CoreModule::staticfile(Context* cx, Params& args) {
  if (cx->tryServeTraceOrigin())
    return true;

  if (cx->request()->directoryDepth() < 0) {
    cx->logError("Directory traversal detected: {}", cx->request()->path());
    return cx->sendErrorPage(HttpStatus::BadRequest);
  }

  HttpStatus status = daemon().fileHandler().handle(cx->request(),
                                                    cx->response(),
                                                    cx->file());
  if (status == HttpStatus::NotFound) {
    return false;
  } else if (!isError(status)) {
    return true;
  } else {
    return cx->sendErrorPage(status);
  }
}

bool CoreModule::precompressed(Context* cx, Params& args) {
  if (cx->tryServeTraceOrigin())
    return true;

  if (cx->request()->directoryDepth() < 0) {
    cx->logError("Directory traversal detected: {}", cx->request()->path());
    return cx->sendErrorPage(HttpStatus::BadRequest);
  }

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

      auto pc = daemon().vfs().getFile(cx->file()->path() + encoding.fileExtension);

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
          return cx->sendErrorPage(status);
        }
      }
    }
  }

  return false;
}

void CoreModule::autoindex(Context* cx, Params& args) {
  if (cx->documentRoot().empty()) {
    cx->logError("autoindex: No document root set yet. Skipping.");
    // error: must have a document-root set first.
    return;
  }

  if (!cx->file()) {
    cx->logDebug("autoindex: No file mapped. Skipping.");
    return;
  }

  if (!cx->file()->isDirectory())
    return;

  const FlowStringArray& indexfiles = args.getStringArray(1);
  for (size_t i = 0, e = indexfiles.size(); i != e; ++i) {
    if (matchIndex(cx, indexfiles[i])) {
      return;
    }
  }
}

bool CoreModule::matchIndex(Context* cx, const std::string& arg) {
  std::string ipath = FileUtil::joinPaths(cx->file()->path(), arg);
  std::string path = FileUtil::joinPaths(cx->documentRoot(), ipath);

  if (auto fi = daemon().vfs().getFile(path)) {
    if (fi->isRegular()) {
      cx->setFile(fi);
      return true;
    }
  }

  return false;
}

void CoreModule::rewrite(Context* cx, Params& args) {
  std::string filepath = FileUtil::joinPaths(cx->documentRoot(),
                                             args.getString(1));
  auto file = daemon().vfs().getFile(filepath);
  cx->setFile(file);
  args.setResult(file ? file->exists() : false);
}

void CoreModule::pathinfo(Context* cx, Params& args) {
  if (!cx->file()) {
    cx->logError("pathinfo: no file information available. "
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
      file = daemon().vfs().getFile(file->path().substr(0, pos));
      cx->setFile(file);
    } else {
      break;
    }
  }
}

void CoreModule::header_add(Context* cx, Params& args) {
  std::string name = args.getString(1);
  std::string value = args.getString(2);

  cx->response()->onPostProcess([cx, name, value]() {
    cx->response()->addHeader(name, value);
  });
}

void CoreModule::header_append(Context* cx, Params& args) {
  std::string name = args.getString(1);
  std::string value = args.getString(2);
  std::string delim = args.getString(3);

  cx->response()->onPostProcess([cx, name, value, delim]() {
    cx->response()->appendHeader(name, value, delim);
  });
}

void CoreModule::header_overwrite(Context* cx, Params& args) {
  std::string name = args.getString(1);
  std::string value = args.getString(2);

  cx->response()->onPostProcess([cx, name, value]() {
    cx->response()->setHeader(name, value);
  });
}

void CoreModule::header_remove(Context* cx, Params& args) {
  std::string name = args.getString(1);

  cx->response()->onPostProcess([cx, name]() {
    cx->response()->removeHeader(name);
  });
}

void CoreModule::expire(Context* cx, Params& args) {
  static const char* timeFormat = "%a, %d %b %Y %H:%M:%S GMT";
  const UnixTime now = cx->now();
  const UnixTime mtime = cx->file() ? cx->file()->mtime() : now;
  const time_t value = args.getInt(1);

  const auto [maxAge, expiry] =
      value < mtime.unixtime() // value is a timespan? (otherwise an absolute time)
          ? std::make_tuple(Duration::fromSeconds(value),
                            now + Duration::fromSeconds(value))
          : std::make_tuple(std::max(now, UnixTime(value * kMicrosPerSecond)) - now,
                            std::max(now, UnixTime(value * kMicrosPerSecond)));

  cx->response()->setHeader("Expires", expiry.format(timeFormat));
  cx->response()->setHeader("Cache-Control", fmt::format("max-age={}", maxAge.seconds()));

  // XXX same result, just more human readable ;-)
  // if (value < mtime.unixtime()) {
  //   // value is a timespan
  //   const Duration maxAge = Duration::fromSeconds(value);
  //   const UnixTime expiry = now + maxAge;
  //
  //   cx->response()->setHeader("Expires", expiry.format(timeFormat));
  //   cx->response()->setHeader("Cache-Control", fmt::format("max-age={}", maxAge.seconds()));
  // } else {
  //   // value is treated as absolute time
  //   const UnixTime expiry = std::max(now, UnixTime(value * kMicrosPerSecond));
  //   const Duration maxAge = expiry - now;
  //
  //   cx->response()->setHeader("Expires", expiry.format(timeFormat));
  //   cx->response()->setHeader("Cache-Control", fmt::format("max-age={}", maxAge.seconds()));
  // }
}

void CoreModule::req_method(Context* cx, Params& args) {
  args.setResult(cx->request()->unparsedMethod());
}

void CoreModule::req_url(Context* cx, Params& args) {
  args.setResult(cx->request()->unparsedUri());
}

void CoreModule::req_path(Context* cx, Params& args) {
  args.setResult(cx->request()->path());
}

void CoreModule::req_query(Context* cx, Params& args) {
  args.setResult(cx->request()->query());
}

void CoreModule::req_header(Context* cx, Params& args) {
  args.setResult(cx->request()->getHeader(args.getString(1)));
}

void CoreModule::req_cookie(Context* cx, Params& args) {
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

void CoreModule::req_host(Context* cx, Params& args) {
  args.setResult(cx->request()->host());
}

void CoreModule::req_pathinfo(Context* cx, Params& args) {
  args.setResult(cx->pathInfo());
}

void CoreModule::req_is_secure(Context* cx, Params& args) {
  args.setResult(cx->request()->isSecure());
}

void CoreModule::req_scheme(Context* cx, Params& args) {
  args.setResult(cx->request()->isSecure() ? "https" : "http");
}

void CoreModule::req_status_code(Context* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->response()->status()));
}

void CoreModule::conn_remote_ip(Context* cx, Params& args) {
  args.setResult(&(cx->remoteIP()));
}

void CoreModule::conn_remote_port(Context* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->remotePort()));
}

void CoreModule::conn_local_ip(Context* cx, Params& args) {
  args.setResult(&(cx->localIP()));
}

void CoreModule::conn_local_port(Context* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->localPort()));
}

void CoreModule::phys_path(Context* cx, Params& args) {
  static const std::string none;
  args.setResult(cx->file() ? cx->file()->path() : none);
}

void CoreModule::phys_exists(Context* cx, Params& args) {
  args.setResult(cx->file() ? cx->file()->exists() : false);
}

void CoreModule::phys_is_reg(Context* cx, Params& args) {
  args.setResult(cx->file() ? cx->file()->isRegular() : false);
}

void CoreModule::phys_is_dir(Context* cx, Params& args) {
  args.setResult(cx->file() ? cx->file()->isDirectory() : false);
}

void CoreModule::phys_is_exe(Context* cx, Params& args) {
  args.setResult(cx->file() ? cx->file()->isExecutable() : false);
}

void CoreModule::phys_mtime(Context* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->file() ? cx->file()->mtime().unixtime() : 0));
}

void CoreModule::phys_size(Context* cx, Params& args) {
  args.setResult(static_cast<FlowNumber>(cx->file() ? cx->file()->size() : 0));
}

void CoreModule::phys_etag(Context* cx, Params& args) {
  static const std::string none;
  args.setResult(cx->file() ? cx->file()->etag() : none);
}

void CoreModule::phys_mimetype(Context* cx, Params& args) {
  static const std::string none;
  args.setResult(cx->file() ? cx->file()->mimetype() : none);
}

void CoreModule::regex_group(Context* cx, Params& args) {
  FlowNumber position = args.getInt(1);

  if (const flow::util::RegExp::Result* rr = cx->runner()->regexpContext()->regexMatch()) {
    if (position >= 0 && position < static_cast<FlowNumber>(rr->size())) {
      std::string match = rr->str(position);
      args.setResult(args.caller()->newString(std::move(match)));
    } else {
      // match index out of bounds
      args.setResult("");
    }
  } else {
    // no regex match executed
    args.setResult("");
  }
}

void CoreModule::req_accept_language(Context* cx, Params& args) {
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

  auto parseToken = [&]() -> std::string {
    const char* beg = i;
    while (i != e && (std::isalnum(*i) || *i == '-' || *i == '_')) {
      ++i;
    }
    return std::string(beg, i);
  };

  auto isSupported = [&](const std::string& language) -> bool {
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

    std::string token = parseToken();
    if (isSupported(token)) {
      args.setResult(args.caller()->newString(std::move(token)));
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

bool CoreModule::verify_req_accept_language(flow::Instr* call, flow::IRBuilder* builder) {
  auto arg = dynamic_cast<ConstantArray*>(call->operand(1));
  assert(arg != nullptr);

  // empty-arrays aren't currently supported, but write the test in case I
  // changed my mind on the other side. ;)
  if (arg->get().size() == 0) {
    logError("req.accept_language() requires a non-empty array argument.");
    return false;
  }

  return true;
}

} // namespace x0d
