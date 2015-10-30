// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "Config.h"
#include "XzeroDaemon.h"
#include "XzeroWorker.h"
#include "XzeroContext.h"

#include "modules/access.h"
#include "modules/accesslog.h"
#include "modules/auth.h"
#include "modules/compress.h"
#include "modules/core.h"
#include "modules/dirlisting.h"
#include "modules/empty_gif.h"
#include "modules/userdir.h"

#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpOutput.h>
#include <xzero/http/HttpFileHandler.h>
#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/net/SslConnector.h>
#include <xzero/net/InetConnector.h>
#include <xzero/net/Server.h>
#include <xzero/cli/CLI.h>
#include <xzero/cli/Flags.h>
#include <xzero/RuntimeError.h>
#include <xzero/MimeTypes.h>
#include <xzero/Application.h>
#include <xzero/StringUtil.h>
#include <xzero/logging.h>
#include <xzero-flow/FlowParser.h>
#include <xzero-flow/IRGenerator.h>
#include <xzero-flow/TargetCodeGenerator.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/PassManager.h>
#include <xzero-flow/transform/UnusedBlockPass.h>
#include <xzero-flow/transform/EmptyBlockElimination.h>
#include <xzero-flow/transform/InstructionElimination.h>
#include <xzero-flow/FlowCallVisitor.h>
#include <iostream>
#include <sstream>
#include <fstream>

#define PACKAGE_VERSION "0.11.0-dev"
#define PACKAGE_HOMEPAGE_URL "https://xzero.io"

using namespace xzero;
using namespace xzero::http;

// XXX variable defined by mimetypes2cc compiler
extern std::unordered_map<std::string, std::string> mimetypes2cc;

namespace x0d {

XzeroDaemon::XzeroDaemon()
    : generation_(1),
      startupTime_(),
      mimetypes_(),
      vfs_(mimetypes_, "/", true, true, false),
      lastWorker_(0),
      workers_(),
      modules_(),
      server_(new Server()),
      unit_(nullptr),
      programIR_(nullptr),
      program_(nullptr),
      main_(nullptr),
      setupApi_(),
      mainApi_(),
      optimizationLevel_(1),
      advertise_(true),
      maxRequestHeaderSize_(16 * 1024),         // 16 KB
      maxRequestHeaderCount_(128),
      requestHeaderBufferSize_(16 * 1024),      // 16 KB
      requestBodyBufferSize_(16 * 1024),        // 16 KB
      http1_(new http1::ConnectionFactory(
          1024,                         // max_request_uri_size      1 K
          16 * 1024 * 1024,             // max_request_body_size    16 M
          100,                          // max_keepalive_requests  100
          Duration::fromSeconds(8))),   // max_keepalive_idle        8 s
      tcpCork_(false),
      tcpNoDelay_(false),
      maxConnections_(512),
      maxReadIdle_(Duration::fromSeconds(60)),
      maxWriteIdle_(Duration::fromSeconds(360)),
      tcpFinTimeout_(Duration::fromSeconds(60)),
      lingering_(Duration::Zero),
      config_(new Config) {

  http1_->setHandler(std::bind(&XzeroDaemon::handleRequest, this,
        std::placeholders::_1, std::placeholders::_2));

  loadModule<CoreModule>();
  loadModule<AccessModule>();
  loadModule<AccesslogModule>();
  loadModule<AuthModule>();
  loadModule<CompressModule>();
  loadModule<DirlistingModule>();
  loadModule<EmptyGifModule>();
  loadModule<UserdirModule>();

  workers_.emplace_back(new XzeroWorker(this, 0, false));
}

XzeroDaemon::~XzeroDaemon() {
}

bool XzeroDaemon::import(
    const std::string& name,
    const std::string& path,
    std::vector<flow::vm::NativeCallback*>* builtins) {

  if (path.empty())
    logDebug("x0d", "Loading plugin \"$0\"", name);
  else
    logDebug("x0d", "Loading plugin \"$0\" from \"$1\"", name, path);

  // TODO actually load the plugin

  return true;
}

// for instant-mode
void XzeroDaemon::loadConfigEasy(const std::string& docroot, int port) {
  std::string flow =
      "handler setup {\n"
      "  listen port: #{port};\n"
      "}\n"
      "\n"
      "handler main {\n"
      "  docroot '#{docroot}';\n"
      "  staticfile;\n"
      "}\n";

  StringUtil::replaceAll(&flow, "#{port}", std::to_string(port));
  StringUtil::replaceAll(&flow, "#{docroot}", docroot);

  loadConfigStream(
      std::unique_ptr<std::istream>(new std::istringstream(flow)),
      "instant-mode.conf");
}

void XzeroDaemon::loadConfigFile(const std::string& configFileName) {
  std::unique_ptr<std::istream> input(new std::ifstream(configFileName));
  loadConfigStream(std::move(input), configFileName);
}

void XzeroDaemon::loadConfigStream(std::unique_ptr<std::istream>&& is,
                                   const std::string& filename) {
  flow::FlowParser parser(
      this,
      std::bind(&XzeroDaemon::import, this, std::placeholders::_1,
                                            std::placeholders::_2,
                                            std::placeholders::_3),
      [](const std::string& msg) {
        logError("x0d", "Configuration file error. $0", msg);
      });

  parser.openStream(std::move(is), filename);
  unit_ = parser.parse();

  flow::IRGenerator irgen;
  irgen.setExports({"setup", "main"});
  irgen.setErrorCallback([&](const std::string& msg) {
    logError("x0d", "$0", msg);
  });

  programIR_ = irgen.generate(unit_.get());

  {
    flow::PassManager pm;

    // mandatory passes
    pm.registerPass(std::make_unique<flow::UnusedBlockPass>());

    // optional passes
    if (optimizationLevel_ >= 1) {
      pm.registerPass(std::make_unique<flow::EmptyBlockElimination>());
      pm.registerPass(std::make_unique<flow::InstructionElimination>());
    }

    pm.run(programIR_.get());
  }

  verify(programIR_.get());

  program_ = flow::TargetCodeGenerator().generate(programIR_.get());
  program_->link(this);

  validateConfig();

  if (!program_->findHandler("setup")) {
    RAISE(RuntimeError, "No setup handler found.");
  }

  main_ = program_->findHandler("main");
}

bool XzeroDaemon::configure() {
  try {
    // free up some resources (not needed anymore)
    programIR_.reset();

    // run setup handler
    program_->findHandler("setup")->run();

    postConfig();

    return true;
  } catch (const RuntimeError& e) {
    if (e == Status::ConfigurationError) {
      logError("x0d", "Configuration failed. $0", e.what());
      return false;
    }

    throw;
  }
}

void XzeroDaemon::postConfig() {
  if (config_->listeners.empty()) {
    RAISE(ConfigurationError, "No listeners configured.");
  }

  for (const ListenerConfig& l: config_->listeners) {
    if (l.ssl) {
      auto ssl = setupConnector<SslConnector>(
          l.bindAddress, l.port, l.backlog,
          l.multiAcceptCount, l.reuseAddr, l.reusePort);
      if (config_->sslContexts.empty()) {
        RAISE(ConfigurationError, "SSL listeners found but no SSL contexts configured.");
      }
      for (const SslContext& cx: config_->sslContexts) {
        ssl->addContext(cx.certfile, cx.keyfile); // TODO: include trustfile & priorities
      }
    } else {
      setupConnector<InetConnector>(
          l.bindAddress, l.port, l.backlog,
          l.multiAcceptCount, l.reuseAddr, l.reusePort);
    }
  }

  if (mimetypes_.empty()) {
    logDebug("x0d", "No mimetypes given. Defaulting to builtin database.");
    mimetypes_.load(mimetypes2cc);
  }
}

void XzeroDaemon::handleRequest(HttpRequest* request, HttpResponse* response) {
  XzeroContext* cx = new XzeroContext(main_, request, response);

  bool handled = cx->run();
  if (!handled) {
    response->setStatus(HttpStatus::NotFound);
    response->completed();
  }
}

void XzeroDaemon::validateConfig() {
  validateContext("setup", setupApi_);
  validateContext("main", mainApi_);
}

void XzeroDaemon::validateContext(const std::string& entrypointHandlerName,
                                  const std::vector<std::string>& api) {
  auto entrypointFn = unit_->findHandler(entrypointHandlerName);
  if (!entrypointFn)
    RAISE(RuntimeError, "No handler with name %s found.",
                        entrypointHandlerName.c_str());

  flow::FlowCallVisitor callVisitor(entrypointFn);
  auto calls = callVisitor.calls();

  unsigned errorCount = 0;

  for (const auto& i: calls) {
    if (!i->callee()->isBuiltin()) {
      // skip script user-defined handlers
      continue;
    }

    if (std::find(api.begin(), api.end(), i->callee()->name()) == api.end()) {
      logError("x0d",
          "Illegal call to '$0' found within handler $1 (or its callees).",
          i->callee()->name(),
          entrypointHandlerName);
      logError("x0d", "$0", i->location().str());
      errorCount++;
    }
  }

  if (errorCount) {
    RAISE(RuntimeError, "Configuration validation failed.");
  }
}

void XzeroDaemon::run() {
  server_->start();

  mainWorker()->runLoop();
  // scheduler_.runLoop();

  server_->stop();
}

template<typename T>
T* XzeroDaemon::setupConnector(
    const xzero::IPAddress& ipaddr, int port,
    int backlog, int multiAccept, bool reuseAddr, bool reusePort) {

  Executor* executor = mainWorker()->scheduler();
  Scheduler* scheduler = mainWorker()->scheduler();
  UniquePtr<ExceptionHandler> eh(new CatchAndLogExceptionHandler("x0d"));

  auto inet = server_->addConnector<T>(
      "inet",
      executor,
      scheduler,
      maxReadIdle_,
      maxWriteIdle_,
      tcpFinTimeout_,
      std::move(eh),
      ipaddr,
      port,
      backlog,
      reuseAddr,
      reusePort
  );

  inet->setMultiAcceptCount(multiAccept);
  inet->addConnectionFactory(http1_);

  return inet;
}

} // namespace x0d
