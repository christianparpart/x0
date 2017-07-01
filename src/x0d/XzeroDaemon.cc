// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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

#include <xzero/sysconfig.h>
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
#include <xzero-flow/ASTPrinter.h>
#include <xzero-flow/FlowParser.h>
#include <xzero-flow/IRGenerator.h>
#include <xzero-flow/TargetCodeGenerator.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/PassManager.h>
#include <xzero-flow/vm/Signature.h>
#include <xzero-flow/vm/Runner.h>
#include <xzero-flow/transform/MergeBlockPass.h>
#include <xzero-flow/transform/UnusedBlockPass.h>
#include <xzero-flow/transform/EmptyBlockElimination.h>
#include <xzero-flow/transform/InstructionElimination.h>
#include <xzero-flow/FlowCallVisitor.h>
#include <iostream>
#include <sstream>
#include <fstream>

using namespace xzero;
using namespace xzero::http;

#if !defined(NDEBUG)
#define TRACE(msg...) logTrace("x0d", msg)
#define DEBUG(msg...) logDebug("x0d", msg)
#else
#define TRACE(msg...) do {} while (0)
#define DEBUG(msg...) do {} while (0)
#endif


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
      eventLoops_(),
      modules_(),
      server_(new Server()),
      program_(),
      main_(),
      setupApi_(),
      mainApi_(),
      optimizationLevel_(1),
      fileHandler_(),
      http1_(),
      configFilePath_(),
      config_(createDefaultConfig()) {

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
std::shared_ptr<flow::vm::Program> XzeroDaemon::loadConfigEasy(
    const std::string& docroot, int port) {
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

  return loadConfigStream(
      std::unique_ptr<std::istream>(new std::istringstream(flow)),
      "instant-mode.conf", false, false, false);
}

std::shared_ptr<xzero::flow::vm::Program> XzeroDaemon::loadConfigFile(
    const std::string& configFileName) {
  return loadConfigFile(configFileName, false, false, false);
}

std::shared_ptr<flow::vm::Program> XzeroDaemon::loadConfigFile(
    const std::string& configFileName,
    bool printAST, bool printIR, bool printTC) {
  configFilePath_ = configFileName;
  std::unique_ptr<std::istream> input(new std::ifstream(configFileName));
  return loadConfigStream(std::move(input), configFileName,
      printAST, printIR, printTC);
}

std::shared_ptr<flow::vm::Program> XzeroDaemon::loadConfigStream(
    std::unique_ptr<std::istream>&& is,
    const std::string& fakeFilename,
    bool printAST, bool printIR, bool printTC) {
  flow::FlowParser parser(
      this,
      std::bind(&XzeroDaemon::import, this, std::placeholders::_1,
                                            std::placeholders::_2,
                                            std::placeholders::_3),
      [](const std::string& msg) {
        logError("x0d", "Configuration file error. $0", msg);
      });

  parser.openStream(std::move(is), fakeFilename);
  std::unique_ptr<flow::Unit> unit = parser.parse();

  validateConfig(unit.get());

  flow::IRGenerator irgen;
  irgen.setExports({"setup", "main"});
  irgen.setErrorCallback([&](const std::string& msg) {
    logError("x0d", "$0", msg);
  });

  std::shared_ptr<flow::IRProgram> programIR = irgen.generate(unit.get());

  patchProgramIR(programIR.get(), &irgen);

  {
    flow::PassManager pm;

    // mandatory passes
    pm.registerPass(std::make_unique<flow::UnusedBlockPass>());

    // optional passes
    if (optimizationLevel_ >= 1) {
      pm.registerPass(std::make_unique<flow::MergeBlockPass>());
    }

    if (optimizationLevel_ >= 2) {
      pm.registerPass(std::make_unique<flow::EmptyBlockElimination>());
    }

    if (optimizationLevel_ >= 3) {
      pm.registerPass(std::make_unique<flow::InstructionElimination>());
    }

    pm.run(programIR.get());
  }

  verify(programIR.get(), &irgen);

  std::shared_ptr<flow::vm::Program> program =
      flow::TargetCodeGenerator().generate(programIR.get());

  program->link(this);

  if (printAST)
    flow::ASTPrinter::print(unit.get());

  if (printIR)
    programIR->dump();

  if (printTC)
    program->dump();

  return program;
}

void XzeroDaemon::patchProgramIR(xzero::flow::IRProgram* programIR,
                                 xzero::flow::IRGenerator* irgen) {
  using namespace xzero::flow;

  IRHandler* mainIR = programIR->findHandler("main");
  irgen->setHandler(mainIR);

  // this function will never return, thus, we're not injecting
  // our return(I)V before the RET instruction but replace it.
  IRBuiltinHandler* returnFn =
      irgen->getBuiltinHandler(vm::Signature("return(II)B"));

  // remove RetInstr if prior instr never returns
  // replace RetInstr with `handler return(II)V 404, 0`
  for (BasicBlock* bb: mainIR->basicBlocks()) {
    if (auto br = dynamic_cast<BrInstr*>(bb->getTerminator())) {
      // check if last instruction *always* finishes the handler
      if (auto handler = dynamic_cast<HandlerCallInstr*>(bb->back(1))) {
        if (handler->callee() == returnFn) { // return(II)B
          delete bb->remove(br);
        }
      }
    } else if (auto ret = dynamic_cast<RetInstr*>(bb->getTerminator())) {
      delete bb->remove(ret);

      // check if last instruction *always* finishes the handler
      if (auto handler = dynamic_cast<HandlerCallInstr*>(bb->back())) {
        if (handler->callee() == returnFn) { // return(II)B
          continue;
        }
      }

      irgen->setInsertPoint(bb);
      irgen->createInvokeHandler(returnFn, { irgen->get(404),   // status
                                             irgen->get(0) });  // statusOverride
    }
  }
}

bool XzeroDaemon::applyConfiguration(std::shared_ptr<flow::vm::Program> program) {
  try {
    program->findHandler("setup")->run();

    // Override main and *then* preserve the program reference.
    // XXX The order is important to not accidentally generate stale weak ptrs.
    main_ = program->findHandler("main");
    program_ = program;

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

std::unique_ptr<Config> XzeroDaemon::createDefaultConfig() {
  std::unique_ptr<Config> config(new Config);

  // defaulting worker/affinities to total host CPU count
  config->workers = CoreModule::cpuCount();
  config->workerAffinities.resize(config->workers);
  for (size_t i = 0; i < config->workers; ++i)
    config->workerAffinities[i] = i;

  return config;
}

void XzeroDaemon::reloadConfiguration() {
  /*
   * 1. suspend the world
   * 2. load new config file
   * 3. run setup handler with producing a diff to what is to be removed
   * 4. Undo anything that's not in setup handler anymore (e.g. tcp listeners)
   * 5. replace main request handler with new one
   * 6. run post-config
   * 6. resume the world
   */

  if (configFilePath_.empty()) {
    logNotice("x0d", "No configuration file given at startup. Nothing to reload.");
    return;
  }

  // reset to config
  config_ = createDefaultConfig();

  try {
    // run setup gracefully
    stopThreads();

    // load new config file into Flow
    std::shared_ptr<flow::vm::Program> program = loadConfigFile(configFilePath_);

    threadedExecutor_.joinAll();
    server_->stop();

    applyConfiguration(program);
  } catch (const std::exception& e) {
    logError("x0d", e, "Error cought while reloading configuration.");
  }
  logNotice("x0d", "Configuration reloading done.");
}

void XzeroDaemon::stopThreads() {
  // suspend all worker threads
  std::for_each(std::next(eventLoops_.begin()), eventLoops_.end(),
                std::bind(&EventLoop::breakLoop, std::placeholders::_1));
  for (size_t i = 1; i < config_->workers; ++i) {
    eventLoops_[i]->unref(); // refers to the startThreads()'s ref()-action
    eventLoops_[i]->breakLoop();
  }
}

void XzeroDaemon::startThreads() {
  // resume all worker threads
  for (size_t i = 1; i < config_->workers; ++i) {
    threadedExecutor_.execute(std::bind(&XzeroDaemon::runOneThread, this, i));
    eventLoops_[i]->ref(); // we ref here to keep the loop running
  }
}

void XzeroDaemon::postConfig() {
  if (config_->listeners.empty()) {
    RAISE(ConfigurationError, "No listeners configured.");
  }

#if defined(XZERO_WSL)
  if (config_->tcpFinTimeout != Duration::Zero) {
    config_->tcpFinTimeout = Duration::Zero;
    logWarning("x0d",
               "Your platform does not support overriding TCP FIN timeout. "
               "Using system defaults.");
  }
#endif

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

  // event loops
  if (eventLoops_.empty())
    eventLoops_.emplace_back(createEventLoop());

  for (size_t i = 1; i < config_->workers; ++i)
    eventLoops_.emplace_back(createEventLoop());

  while (eventLoops_.size() > config_->workers)
    eventLoops_.erase(std::prev(eventLoops_.end()));

  // listeners
  server_->removeAllConnectors();
  for (const ListenerConfig& l: config_->listeners) {
    if (l.ssl) {
      if (config_->sslContexts.empty()) {
        RAISE(ConfigurationError, "SSL listeners found but no SSL contexts configured.");
      }
      logNotice("x0d", "Starting HTTPS listener on $0:$1", l.bindAddress, l.port);
      setupConnector<SslConnector>(
          l.bindAddress, l.port, l.backlog,
          l.multiAcceptCount, l.reuseAddr, l.deferAccept, l.reusePort,
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
          l.multiAcceptCount, l.reuseAddr, l.deferAccept, l.reusePort,
          nullptr);
    }
  }

  for (std::unique_ptr<XzeroModule>& module: modules_) {
    module->onPostConfig();
  }

  server_->start();
  startThreads();
}

std::unique_ptr<EventLoop> XzeroDaemon::createEventLoop() {
  size_t i = eventLoops_.size();

  return std::unique_ptr<EventLoop>(new NativeScheduler(
        std::unique_ptr<xzero::ExceptionHandler>(
              new CatchAndLogExceptionHandler(StringUtil::format("x0d/$0", i))),
        nullptr /* preInvoke */,
        nullptr /* postInvoke */));
}

void XzeroDaemon::handleRequest(HttpRequest* request, HttpResponse* response) {
  // XXX the XzeroContext instance is getting deleted upon response completion
  XzeroContext* cx = new XzeroContext(main_,
                                      request,
                                      response,
                                      &config_->errorPages,
                                      config_->maxInternalRedirectCount);

  if (request->expect100Continue()) {
    response->send100Continue([cx, request](bool succeed) {
      request->consumeContent(std::bind(&flow::vm::Runner::run, cx->runner()));
    });
  } else {
    request->consumeContent(std::bind(&flow::vm::Runner::run, cx->runner()));
  }
}

void XzeroDaemon::validateConfig(flow::Unit* unit) {
  validateContext("setup", setupApi_, unit);
  validateContext("main", mainApi_, unit);
}

void XzeroDaemon::validateContext(const std::string& entrypointHandlerName,
                                  const std::vector<std::string>& api,
                                  flow::Unit* unit) {
  auto entrypointFn = unit->findHandler(entrypointHandlerName);
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
  eventHandler_.reset(new XzeroEventHandler(this, eventLoops_[0].get()));
  runOneThread(0);
  TRACE("Main loop quit. Shutting down.");
  server_->stop();
}

void XzeroDaemon::runOneThread(size_t index) {
  EventLoop* eventLoop = eventLoops_[index].get();

  if (index < config_->workerAffinities.size())
    setThreadAffinity(config_->workerAffinities[index], index);

  TRACE("worker/$0: Event loop enter", index);
  eventLoop->runLoop();
  TRACE("worker/$0: Event loop terminated.", index);
}

void XzeroDaemon::setThreadAffinity(int cpu, int workerId) {
#if defined(HAVE_DECL_PTHREAD_SETAFFINITY_NP) && HAVE_DECL_PTHREAD_SETAFFINITY_NP
  cpu_set_t set;

  CPU_ZERO(&set);
  CPU_SET(cpu, &set);

  TRACE("setAffinity: cpu $0 on worker $1", cpu, workerId);

  pthread_t tid = pthread_self();

  int rv = pthread_setaffinity_np(tid, sizeof(set), &set);
  if (rv < 0) {
    logError("x0d",
             "setting event-loopaffinity on CPU $0 failed for worker $1. $2",
             cpu, workerId, strerror(errno));
  }
#else
  logWarning("x0d",
           "setting event-loop affinity on CPU $0 failed for worker $1. $2",
           cpu, workerId, strerror(ENOTSUP));
#endif
}

void XzeroDaemon::terminate() {
  terminate_ = true;

  for (auto& eventLoop: eventLoops_) {
    eventLoop->breakLoop();
  }
}

Executor* XzeroDaemon::selectClientExecutor() {
  // TODO: support least-load

  if (++lastWorker_ >= eventLoops_.size())
    lastWorker_ = 0;

  TRACE("select client scheduler $0", lastWorker_);

  return eventLoops_[lastWorker_].get();
}

template<typename T>
void XzeroDaemon::setupConnector(
    const xzero::IPAddress& bindAddress, int port, int backlog,
    int multiAcceptCount, bool reuseAddr, bool deferAccept, bool reusePort,
    std::function<void(T*)> connectorVisitor) {

  if (reusePort && !InetConnector::isReusePortSupported()) {
    logWarning("x0d", "Your platform does not support SO_REUSEPORT. "
                      "Falling back to traditional connection scheduling.");
    reusePort = false;
  }

  if (deferAccept && !InetConnector::isDeferAcceptSupported()) {
    logWarning("x0d", "Your platform does not support TCP_DEFER_ACCEPT. "
                      "Disabling.");
    deferAccept = false;
  }

  if (reusePort) {
    for (auto& eventLoop: eventLoops_) {
      EventLoop* loop = eventLoop.get();
      T* connector = doSetupConnector<T>(
          loop,
          [loop]() -> Executor* { return loop; },
          bindAddress, port, backlog,
          multiAcceptCount, reuseAddr, deferAccept, reusePort);
      if (connectorVisitor) {
        connectorVisitor(connector);
      }
    }
  } else {
    T* connector = doSetupConnector<T>(
        eventLoops_[0].get(),
        std::bind(&XzeroDaemon::selectClientExecutor, this),
        bindAddress, port, backlog,
        multiAcceptCount, reuseAddr, deferAccept, reusePort);
    if (connectorVisitor) {
      connectorVisitor(connector);
    }
  }
}

template<typename T>
T* XzeroDaemon::doSetupConnector(
    xzero::Executor* executor,
    xzero::InetConnector::ExecutorSelector clientExecutorSelector,
    const xzero::IPAddress& ipaddr, int port, int backlog,
    int multiAccept, bool reuseAddr, bool deferAccept, bool reusePort) {

  auto inet = server_->addConnector<T>(
      "inet",
      executor,
      clientExecutorSelector,
      config_->maxReadIdle,
      config_->maxWriteIdle,
      config_->tcpFinTimeout,
      ipaddr,
      port,
      backlog,
      reuseAddr,
      reusePort
  );

  if (deferAccept)
    inet->setDeferAccept(deferAccept);

  inet->setMultiAcceptCount(multiAccept);
  inet->addConnectionFactory(http1_->protocolName(),
      std::bind(&HttpConnectionFactory::create, http1_.get(),
                std::placeholders::_1,
                std::placeholders::_2));

  return inet;
}

} // namespace x0d
