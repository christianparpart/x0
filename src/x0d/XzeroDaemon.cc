// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#include <x0d/Config.h>
#include <x0d/XzeroDaemon.h>
#include <x0d/XzeroEventHandler.h>
#include <x0d/XzeroContext.h>

#include "modules/access.h"
#include "modules/accesslog.h"
#include "modules/auth.h"
#include "modules/compress.h"
#include "modules/core.h"
#include "modules/dirlisting.h"
#include "modules/empty_gif.h"
#include "modules/userdir.h"
#include "modules/webdav.h"
#include "modules/proxy.h"

#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
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

using namespace xzero;
using namespace xzero::http;

// XXX variable defined by mimetypes2cc compiler
extern std::unordered_map<std::string, std::string> mimetypes2cc;

namespace x0d {

XzeroDaemon::XzeroDaemon()
    : generation_(1),
      startupTime_(),
      terminate_(false),
      eventHandler_(),
      mimetypes_(),
      vfs_(mimetypes_, "/", true, true, false),
      lastWorker_(0),
      schedulers_(),
      modules_(),
      server_(new Server()),
      unit_(nullptr),
      programIR_(nullptr),
      program_(nullptr),
      main_(nullptr),
      setupApi_(),
      mainApi_(),
      optimizationLevel_(1),
      fileHandler_(),
      http1_(),
      config_(new Config) {

  // defaulting worker/affinities to total host CPU count
  config_->workers = CoreModule::cpuCount();
  config_->workerAffinities.resize(config_->workers);
  for (int i = 0; i < config_->workers; ++i)
    config_->workerAffinities[i] = i;

  loadModule<AccessModule>();
  loadModule<AccesslogModule>();
  loadModule<AuthModule>();
  loadModule<CompressModule>();
  loadModule<CoreModule>();
  loadModule<DirlistingModule>();
  loadModule<EmptyGifModule>();
  loadModule<ProxyModule>();
  loadModule<UserdirModule>();
  loadModule<WebdavModule>();
}

XzeroDaemon::~XzeroDaemon() {
  terminate();
  threadedExecutor_.joinAll();
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

  // HTTP/1 connection factory
  http1_.reset(new http1::ConnectionFactory(
      config_->requestHeaderBufferSize,
      config_->requestBodyBufferSize,
      config_->maxRequestUriLength,
      config_->maxRequestBodySize,
      config_->maxKeepAliveRequests,
      config_->maxKeepAlive,
      config_->tcpCork,
      config_->tcpNoDelay));

  http1_->setHandler(std::bind(&XzeroDaemon::handleRequest, this,
        std::placeholders::_1, std::placeholders::_2));

  // mimetypes
  mimetypes_.setDefaultMimeType(config_->mimetypesDefault);

  if (!config_->mimetypesPath.empty()) {
    mimetypes_.loadFromLocal(config_->mimetypesPath);
  }

  if (mimetypes_.empty()) {
    logDebug("x0d", "No mimetypes given. Defaulting to builtin database.");
    mimetypes_.load(mimetypes2cc);
  }

  // schedulers & threading
  schedulers_.emplace_back(newScheduler());

  for (int i = 1; i < config_->workers; ++i) {
    schedulers_.emplace_back(newScheduler());
    threadedExecutor_.execute(std::bind(&XzeroDaemon::runOneThread, this, i));
  }

  // listeners
  for (const ListenerConfig& l: config_->listeners) {
    if (l.ssl) {
      if (config_->sslContexts.empty()) {
        RAISE(ConfigurationError, "SSL listeners found but no SSL contexts configured.");
      }
      logNotice("x0d", "Starting HTTPS listener on $0:$1", l.bindAddress, l.port);
      setupConnector<SslConnector>(
          l.bindAddress, l.port, l.backlog,
          l.multiAcceptCount, l.reuseAddr, l.reusePort,
          [this](SslConnector* connector) {
            for (const SslContext& cx: config_->sslContexts) {
              // TODO: include trustfile & priorities
              connector->addContext(cx.certfile, cx.keyfile);
            }
          }
      );
    } else {
      logNotice("x0d", "Starting HTTP listener on $0:$1", l.bindAddress, l.port);
      setupConnector<InetConnector>(
          l.bindAddress, l.port, l.backlog,
          l.multiAcceptCount, l.reuseAddr, l.reusePort,
          nullptr);
    }
  }

  for (XzeroModule* module: modules_) {
    module->onPostConfig();
  }

  eventHandler_.reset(new XzeroEventHandler(this, schedulers_[0].get()));
}

std::unique_ptr<Scheduler> XzeroDaemon::newScheduler() {
  return std::unique_ptr<Scheduler>(new NativeScheduler(
        std::unique_ptr<xzero::ExceptionHandler>(
              new CatchAndLogExceptionHandler("x0d")),
        nullptr /* preInvoke */,
        nullptr /* postInvoke */));
}

void XzeroDaemon::handleRequest(HttpRequest* request, HttpResponse* response) {
  XzeroContext* cx = new XzeroContext(main_, request, response);
  cx->run();
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
  runOneThread(0);
  logTrace("x0d", "Main loop quit. Shutting down.");
  server_->stop();
}

void XzeroDaemon::runOneThread(int index) {
  Scheduler* scheduler = schedulers_[index].get();

  if (index <= config_->workerAffinities.size())
    setThreadAffinity(config_->workerAffinities[index], index);

  while (!terminate_.load()) {
    scheduler->runLoop();
  }
}

void XzeroDaemon::setThreadAffinity(int cpu, int workerId) {
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
  cpu_set_t set;

  CPU_ZERO(&set);
  CPU_SET(cpu, &set);

  logTrace("x0d", "setAffinity: cpu $0 on worker $1", cpu, workerId);

  pthread_t tid = pthread_self();

  int rv = pthread_setaffinity_np(tid, sizeof(set), &set);
  if (rv < 0) {
    logError("x0d",
             "setting scheduler affinity on CPU $0 failed for worker $1. $2",
             cpu, workerId, strerror(errno));
  }
#else
  logError("x0d",
           "setting scheduler affinity on CPU $0 failed for worker $1. $2",
           cpu, workerId, strerror(ENOTSUP));
#endif
}

void XzeroDaemon::terminate() {
  terminate_ = true;

  for (auto& scheduler: schedulers_) {
    scheduler->breakLoop();
  }
}

Scheduler* XzeroDaemon::selectClientScheduler() {
  // TODO: support least-load

  if (++lastWorker_ >= schedulers_.size())
    lastWorker_ = 0;

  return schedulers_[lastWorker_].get();
}

template<typename T>
void XzeroDaemon::setupConnector(
    const xzero::IPAddress& bindAddress, int port, int backlog,
    int multiAcceptCount, bool reuseAddr, bool reusePort,
    std::function<void(T*)> connectorVisitor) {

  if (reusePort && !InetConnector::isReusePortSupported()) {
    logWarning("x0d", "You platform does not support SO_REUSEPORT. "
                      "Falling back to traditional connection scheduling.");
    reusePort = false;
  }

  if (reusePort) {
    for (auto& scheduler: schedulers_) {
      Scheduler* sched = scheduler.get();
      T* connector = doSetupConnector<T>(
          sched,
          [sched]() -> Scheduler* { return sched; },
          bindAddress, port, backlog,
          multiAcceptCount, reuseAddr, reusePort);
      if (connectorVisitor) {
        connectorVisitor(connector);
      }
    }
  } else {
    T* connector = doSetupConnector<T>(
        schedulers_[0].get(),
        std::bind(&XzeroDaemon::selectClientScheduler, this),
        bindAddress, port, backlog,
        multiAcceptCount, reuseAddr, reusePort);
    if (connectorVisitor) {
      connectorVisitor(connector);
    }
  }
}

template<typename T>
T* XzeroDaemon::doSetupConnector(
    xzero::Scheduler* listenerScheduler,
    xzero::InetConnector::SchedulerSelector clientSchedulerSelector,
    const xzero::IPAddress& ipaddr, int port,
    int backlog, int multiAccept, bool reuseAddr, bool reusePort) {

  auto inet = server_->addConnector<T>(
      "inet",
      listenerScheduler,
      listenerScheduler,
      clientSchedulerSelector,
      config_->maxReadIdle,
      config_->maxWriteIdle,
      config_->tcpFinTimeout,
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
