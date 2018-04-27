// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <x0d/Config.h>
#include <x0d/Daemon.h>
#include <x0d/Context.h>

#include "modules/access.h"
#include "modules/accesslog.h"
#include "modules/auth.h"
#include "modules/compress.h"
#include "modules/core.h"
#include "modules/dirlisting.h"
#include "modules/empty_gif.h"
#include "modules/userdir.h"
#include "modules/webdav.h"

#if defined(ENABLE_PROXY)
#include "modules/proxy.h"
#endif

#include <xzero/sysconfig.h>
#include <xzero/Application.h>
#include <xzero/UnixSignalInfo.h>
#include <xzero/http/HttpRequest.h>
#include <xzero/http/HttpResponse.h>
#include <xzero/http/HttpFileHandler.h>
#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/RuntimeError.h>
#include <xzero/MimeTypes.h>
#include <xzero/StringUtil.h>
#include <xzero/WallClock.h>
#include <xzero/logging.h>
#include <xzero-flow/ASTPrinter.h>
#include <xzero-flow/FlowParser.h>
#include <xzero-flow/IRGenerator.h>
#include <xzero-flow/Signature.h>
#include <xzero-flow/TargetCodeGenerator.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/PassManager.h>
#include <xzero-flow/vm/Runner.h>
#include <xzero-flow/transform/MergeBlockPass.h>
#include <xzero-flow/transform/UnusedBlockPass.h>
#include <xzero-flow/transform/EmptyBlockElimination.h>
#include <xzero-flow/transform/InstructionElimination.h>
#include <xzero-flow/FlowCallVisitor.h>
#include <fmt/format.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <signal.h>

#if defined(XZERO_ENABLE_SSL)
#include <xzero/net/SslConnector.h>
#endif

using namespace xzero;
using namespace xzero::http;

// XXX variable defined by mimetypes2cc compiler
extern std::unordered_map<std::string, std::string> mimetypes2cc;

namespace x0d {

Daemon::Daemon()
    : generation_(1),
      startupTime_(WallClock::now()),
      terminate_(false),
      mimetypes_(),
      vfs_(mimetypes_, "/", true, true, false),
      lastWorker_(0),
      eventLoops_(),
      modules_(),
      connectors_(),
      program_(),
      main_(),
      setupApi_(),
      mainApi_(),
      optimizationLevel_(1),
      fileHandler_(),
      http1_(),
      configFilePath_(),
      config_(createDefaultConfig()),
      state_(DaemonState::Inactive) {
  // main event loop is always available
  eventLoops_.emplace_back(createEventLoop());

  // setup singal handling
#if !defined(XZERO_OS_WINDOWS)
  mainEventLoop()->executeOnSignal(SIGHUP, [&](const auto& si) { onConfigReloadSignal(si); });
  //mainEventLoop()->executeOnSignal(SIGHUP, std::bind(&Daemon::onConfigReloadSignal, this, std::placeholders::_1));
  mainEventLoop()->executeOnSignal(SIGUSR1, std::bind(&Daemon::onCycleLogsSignal, this, std::placeholders::_1));
  mainEventLoop()->executeOnSignal(SIGUSR2, std::bind(&Daemon::onUpgradeBinarySignal, this, std::placeholders::_1));
  mainEventLoop()->executeOnSignal(SIGQUIT, std::bind(&Daemon::onGracefulShutdownSignal, this, std::placeholders::_1));
#endif
  mainEventLoop()->executeOnSignal(SIGTERM, std::bind(&Daemon::onQuickShutdownSignal, this, std::placeholders::_1));
  mainEventLoop()->executeOnSignal(SIGINT, std::bind(&Daemon::onQuickShutdownSignal, this, std::placeholders::_1));

  loadModule<AccessModule>();
  loadModule<AccesslogModule>();
  loadModule<AuthModule>();
  loadModule<CompressModule>();
  loadModule<CoreModule>();
  loadModule<DirlistingModule>();
  loadModule<EmptyGifModule>();

#if defined(ENABLE_PROXY)
  loadModule<ProxyModule>();
#endif

  loadModule<UserdirModule>();
  loadModule<WebdavModule>();
}

Daemon::~Daemon() {
  terminate();
  threadedExecutor_.joinAll();
}

bool Daemon::import(
    const std::string& name,
    const std::string& path,
    std::vector<flow::NativeCallback*>* builtins) {

  if (path.empty())
    logDebug("Loading plugin \"{}\"", name);
  else
    logDebug("Loading plugin \"{}\" from \"{}\"", name, path);

  // TODO actually load the plugin

  return true;
}

// for instant-mode
std::unique_ptr<flow::Program> Daemon::loadConfigEasy(
    const std::string& docroot, int port) {
  return loadConfigEasy(docroot, port, false, false, false);
}

std::unique_ptr<flow::Program> Daemon::loadConfigEasy(
    const std::string& docroot, int port,
    bool printAST, bool printIR, bool printTC) {
  std::string flow =
      "handler setup {\n"
      "  listen port: #{port};\n"
      "}\n"
      "\n"
      "handler main {\n"
      "  accesslog '/dev/stdout', 'combined';\n"
      "  docroot '#{docroot}';\n"
      "  staticfile;\n"
      "}\n";

  StringUtil::replaceAll(&flow, "#{port}", std::to_string(port));
  StringUtil::replaceAll(&flow, "#{docroot}", docroot);

  return loadConfigStream(
      std::make_unique<std::istringstream>(flow),
      "instant-mode.conf", printAST, printIR, printTC);
}

std::unique_ptr<flow::Program> Daemon::loadConfigFile(
    const std::string& configFileName) {
  return loadConfigFile(configFileName, false, false, false);
}

std::unique_ptr<flow::Program> Daemon::loadConfigFile(
    const std::string& configFileName,
    bool printAST, bool printIR, bool printTC) {
  configFilePath_ = configFileName;
  return loadConfigStream(
      std::make_unique<std::ifstream>(configFileName), configFileName,
      printAST, printIR, printTC);
}

std::unique_ptr<flow::Program> Daemon::loadConfigStream(
    std::unique_ptr<std::istream>&& is,
    const std::string& fakeFilename,
    bool printAST, bool printIR, bool printTC) {
  flow::diagnostics::Report report;
  flow::FlowParser parser(
      &report,
      this,
      std::bind(&Daemon::import, this, std::placeholders::_1,
                                            std::placeholders::_2,
                                            std::placeholders::_3));

  parser.openStream(std::move(is), fakeFilename);
  std::unique_ptr<flow::UnitSym> unit = parser.parse();

  validateConfig(unit.get());

  report.log();
  report.clear();

  if (printAST) {
    flow::ASTPrinter::print(unit.get());
    return nullptr;
  }

  flow::IRGenerator irgen{[this](const std::string& msg) { logError("{}", msg); },
                          {"setup", "main"}};

  std::shared_ptr<flow::IRProgram> programIR = irgen.generate(unit.get());

  patchProgramIR(programIR.get(), &irgen);

  verifyNativeCalls(programIR.get(), &irgen);

  {
    flow::PassManager pm;

    // mandatory passes
    pm.registerPass(std::make_unique<flow::UnusedBlockPass>());

    // optional passes
    if (optimizationLevel_ >= 1) {
      pm.registerPass(std::make_unique<flow::MergeBlockPass>());
      pm.registerPass(std::make_unique<flow::EmptyBlockElimination>());
      pm.registerPass(std::make_unique<flow::InstructionElimination>());
    }

    pm.run(programIR.get());
  }

  if (printIR) {
    programIR->dump();
    return nullptr;
  }

  std::unique_ptr<flow::Program> program =
      flow::TargetCodeGenerator().generate(programIR.get());

  program->link(this, &report);
  report.log();

  if (printTC)
    program->dump();

  return program;
}

void Daemon::patchProgramIR(flow::IRProgram* programIR,
                            flow::IRGenerator* irgen) {
  using namespace flow;

  IRHandler* mainIR = programIR->findHandler("main");
  irgen->setHandler(mainIR);

  // this function will never return, thus, we're not injecting
  // our return(I)V before the RET instruction but replace it.
  IRBuiltinHandler* returnFn =
      irgen->findBuiltinHandler(Signature("return(II)B"));

  // remove RetInstr if prior instr never returns
  // replace RetInstr with `handler return(II)V 404, 0`
  for (BasicBlock* bb: mainIR->basicBlocks()) {
    if (auto br = dynamic_cast<BrInstr*>(bb->getTerminator())) {
      // check if last instruction *always* finishes the handler
      if (auto handler = dynamic_cast<HandlerCallInstr*>(bb->back(1))) {
        if (handler->callee() == returnFn) { // return(II)B
          bb->remove(br);
        }
      }
    } else if (auto ret = dynamic_cast<RetInstr*>(bb->getTerminator())) {
      bb->remove(ret);

      // check if last instruction *always* finishes the handler
      if (auto handler = dynamic_cast<HandlerCallInstr*>(bb->back())) {
        if (handler->callee() == returnFn) { // return(II)B
          continue;
        }
      }

      irgen->setInsertPoint(bb);
      irgen->createInvokeHandler(returnFn, { irgen->get(404),   // status
                                             irgen->get(0) });  // statusOverride

      // XXX every basic block *must* have one terminate instr at the end
      // and since the returnFn handler doesn't hint that, we've to add this
      // here (but will never be reached).
      //
      // TODO: we could add an attribute to native handlers, so we can
      // distinguish between never-returning handlers and those who may do.
      irgen->createRet(irgen->get(false));
    }
  }
}

void Daemon::applyConfiguration(std::unique_ptr<flow::Program>&& program) {
  program->findHandler("setup")->run();

  // Override main and *then* preserve the program reference.
  // XXX The order is important to not accidentally generate stale weak ptrs.
  main_ = program->findHandler("main");
  program_ = std::move(program);

  postConfig();
}

void Daemon::start() {
  for (auto& connector: connectors_) {
    connector->start();
  }
}

void Daemon::stop() {
  for (auto& connector: connectors_) {
    connector->stop();
  }
}

void Daemon::removeAllConnectors() {
  while (!connectors_.empty()) {
    connectors_.erase(std::prev(connectors_.end()));
  }
}

std::unique_ptr<Config> Daemon::createDefaultConfig() {
  std::unique_ptr<Config> config = std::make_unique<Config>();

  // defaulting worker/affinities to total host CPU count
  config->workers = CoreModule::cpuCount();
  config->workerAffinities.resize(config->workers);
  for (size_t i = 0; i < config->workers; ++i)
    config->workerAffinities[i] = i;

  return config;
}

void Daemon::reloadConfiguration() {
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
    logNotice("No configuration file given at startup. Nothing to reload.");
    return;
  }

  // reset to config
  config_ = createDefaultConfig();

  try {
    // run setup gracefully
    stopThreads();

    // load new config file into Flow
    std::unique_ptr<flow::Program> program = loadConfigFile(configFilePath_);

    threadedExecutor_.joinAll();
    stop();

    applyConfiguration(std::move(program));
  } catch (const std::exception& e) {
    logFatal("Error cought while reloading configuration. {}", e.what());
  }
  logNotice("Configuration reloading done.");
}

void Daemon::stopThreads() {
  // suspend all worker threads
  std::for_each(std::next(eventLoops_.begin()), eventLoops_.end(),
                std::bind(&EventLoop::breakLoop, std::placeholders::_1));
  for (size_t i = 1; i < config_->workers; ++i) {
    eventLoops_[i]->unref(); // refers to the startThreads()'s ref()-action
    eventLoops_[i]->breakLoop();
  }
}

void Daemon::startThreads() {
  // resume all worker threads
  for (size_t i = 1; i < config_->workers; ++i) {
    threadedExecutor_.execute(std::bind(&Daemon::runOneThread, this, i));
    eventLoops_[i]->ref(); // we ref here to keep the loop running
  }
}

void Daemon::postConfig() {
  if (config_->listeners.empty()) {
    throw ConfigurationError{"No listeners configured."};
  }

  if (config_->tcpFinTimeout != Duration::Zero && Application::isWSL()) {
    config_->tcpFinTimeout = Duration::Zero;
    logWarning("Your platform does not support overriding TCP FIN timeout. "
               "Using system defaults.");
  }

  // HTTP/1 connection factory
  http1_ = std::make_unique<http1::ConnectionFactory>(
      config_->requestHeaderBufferSize,
      config_->requestBodyBufferSize,
      config_->maxRequestUriLength,
      config_->maxRequestBodySize,
      config_->maxKeepAliveRequests,
      config_->maxKeepAlive,
      config_->tcpCork,
      config_->tcpNoDelay);

  http1_->setHandlerFactory(std::bind(&Daemon::createHandler, this,
        std::placeholders::_1, std::placeholders::_2));

  // mimetypes
  if (!config_->mimetypesPath.empty()) {
    mimetypes_ = std::move(MimeTypes{config_->mimetypesDefault, config_->mimetypesPath});
  }

  if (mimetypes_.empty()) {
    logDebug("No mimetypes given. Defaulting to builtin database.");
    mimetypes_ = std::move(MimeTypes{config_->mimetypesDefault, mimetypes2cc});
  }

  // event loops
  for (size_t i = 1; i < config_->workers; ++i)
    eventLoops_.emplace_back(createEventLoop());

  while (eventLoops_.size() > config_->workers)
    eventLoops_.erase(std::prev(eventLoops_.end()));

  // listeners
  removeAllConnectors();
  for (const ListenerConfig& l: config_->listeners) {
    if (l.ssl) {
      if (config_->sslContexts.empty())
        throw ConfigurationError{"SSL listeners found but no SSL contexts configured."};

#if defined(XZERO_ENABLE_SSL)
      logNotice("Starting HTTPS listener on {}:{}", l.bindAddress, l.port);
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
#else
      throw ConfigurationError{fmt::format("Listening on HTTPS for {}:{} not supported in this build.", l.bindAddress, l.port)};
#endif
    } else {
      logNotice("Starting HTTP listener on {}:{}", l.bindAddress, l.port);
      setupConnector<TcpConnector>(
          l.bindAddress, l.port, l.backlog,
          l.multiAcceptCount, l.reuseAddr, l.deferAccept, l.reusePort,
          nullptr);
    }
  }

  for (std::unique_ptr<Module>& module: modules_) {
    module->onPostConfig();
  }

  start();
  startThreads();
}

std::unique_ptr<EventLoop> Daemon::createEventLoop() {
  size_t i = eventLoops_.size();

  return std::make_unique<NativeScheduler>(
        CatchAndLogExceptionHandler{fmt::format("x0d/{}", i)});
}

std::function<void()> Daemon::createHandler(HttpRequest* request,
                                            HttpResponse* response) {
  return Context{main_,
                      request,
                      response,
                      &config_->errorPages,
                      config_->maxInternalRedirectCount};
}

void Daemon::validateConfig(flow::UnitSym* unit) {
  validateContext("setup", setupApi_, unit);
  validateContext("main", mainApi_, unit);
}

void Daemon::validateContext(const std::string& entrypointHandlerName,
                                  const std::vector<std::string>& api,
                                  flow::UnitSym* unit) {
  auto entrypointFn = unit->findHandler(entrypointHandlerName);
  if (!entrypointFn)
      throw ConfigurationError{fmt::format("No handler with name {} found.",
                                           entrypointHandlerName)};

  flow::FlowCallVisitor callVisitor(entrypointFn);
  auto calls = callVisitor.calls();

  unsigned errorCount = 0;

  for (const auto& i: calls) {
    if (!i->callee()->isBuiltin()) {
      // skip script user-defined handlers
      continue;
    }

    if (std::find(api.begin(), api.end(), i->callee()->name()) == api.end()) {
      logError("Illegal call to '{}' found within handler {} (or its callees).",
               i->callee()->name(),
               entrypointHandlerName);
      logError(i->location().str());
      errorCount++;
    }
  }

  if (errorCount) {
    throw ConfigurationError{"Configuration validation failed."};
  }
}

void Daemon::run() {
  runOneThread(0);
  stop();
}

void Daemon::runOneThread(size_t index) {
  EventLoop* eventLoop = eventLoops_[index].get();

  if (index < config_->workerAffinities.size())
    setThreadAffinity(config_->workerAffinities[index], index);

  eventLoop->runLoop();
}

void Daemon::setThreadAffinity(int cpu, int workerId) {
#if defined(HAVE_DECL_PTHREAD_SETAFFINITY_NP) && HAVE_DECL_PTHREAD_SETAFFINITY_NP
  cpu_set_t set;

  CPU_ZERO(&set);
  CPU_SET(cpu, &set);

  pthread_t tid = pthread_self();

  int rv = pthread_setaffinity_np(tid, sizeof(set), &set);
  if (rv < 0) {
    logError("setting event-loopaffinity on CPU {} failed for worker {}. {}",
             cpu, workerId, std::make_error_code(static_cast<std::errc>(errno)).message());
  }
#else
  logWarning("setting event-loop affinity on CPU {} failed for worker {}. {}",
             cpu, workerId, std::make_error_code(std::errc::not_supported).message());
#endif
}

void Daemon::terminate() {
  terminate_ = true;

  for (auto& eventLoop: eventLoops_) {
    eventLoop->breakLoop();
  }
}

Executor* Daemon::selectClientExecutor() {
  // TODO: support least-load

  if (++lastWorker_ >= eventLoops_.size())
    lastWorker_ = 0;

  return eventLoops_[lastWorker_].get();
}

template<typename T>
void Daemon::setupConnector(
    const xzero::IPAddress& bindAddress, int port, int backlog,
    int multiAcceptCount, bool reuseAddr, bool deferAccept, bool reusePort,
    std::function<void(T*)> connectorVisitor) {

  if (reusePort && !TcpConnector::isReusePortSupported()) {
    logWarning("Your platform does not support SO_REUSEPORT. "
               "Falling back to traditional connection scheduling.");
    reusePort = false;
  }

  if (deferAccept && !TcpConnector::isDeferAcceptSupported()) {
    logWarning("Your platform does not support TCP_DEFER_ACCEPT. "
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
        mainEventLoop(),
        std::bind(&Daemon::selectClientExecutor, this),
        bindAddress, port, backlog,
        multiAcceptCount, reuseAddr, deferAccept, reusePort);
    if (connectorVisitor) {
      connectorVisitor(connector);
    }
  }
}

template<typename T>
T* Daemon::doSetupConnector(
    xzero::Executor* executor,
    xzero::TcpConnector::ExecutorSelector clientExecutorSelector,
    const xzero::IPAddress& ipaddr, int port, int backlog,
    int multiAccept, bool reuseAddr, bool deferAccept, bool reusePort) {

  std::unique_ptr<T> inet = std::make_unique<T>(
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
      reusePort);

  if (deferAccept)
    inet->setDeferAccept(deferAccept);

  inet->setMultiAcceptCount(multiAccept);
  inet->addConnectionFactory(http1_->protocolName(),
      std::bind(&HttpConnectionFactory::create, http1_.get(),
                std::placeholders::_1,
                std::placeholders::_2));

  connectors_.emplace_back(std::move(inet));
  return static_cast<T*>(connectors_.back().get());
}

void Daemon::onConfigReloadSignal(const xzero::UnixSignalInfo& info) {
  logNotice("Reloading configuration. (requested via {} by UID {} PID {})",
            PosixSignals::toString(info.signal),
            info.uid.value_or(-1),
            info.pid.value_or(-1));

  /* reloadConfiguration(); */

  mainEventLoop()->executeOnSignal(info.signal, std::bind(&Daemon::onConfigReloadSignal, this, std::placeholders::_1));
}

void Daemon::onCycleLogsSignal(const xzero::UnixSignalInfo& info) {
  logNotice("Cycling logs. (requested via {} by UID {} PID {})",
            PosixSignals::toString(info.signal),
            info.uid.value_or(-1),
            info.pid.value_or(-1));

  onCycleLogs();

  mainEventLoop()->executeOnSignal(info.signal, std::bind(&Daemon::onCycleLogsSignal, this, std::placeholders::_1));
}

void Daemon::onUpgradeBinarySignal(const UnixSignalInfo& info) {
  logNotice("Upgrading binary. (requested via {} by UID {} PID {})",
            PosixSignals::toString(info.signal),
            info.uid.value_or(-1),
            info.pid.value_or(-1));

  /* TODO [x0d] binary upgrade
   * 1. suspend the world
   * 2. save state into temporary file with an inheriting file descriptor
   * 3. exec into new binary
   * 4. (new process) load state from file descriptor and close fd
   * 5. (new process) resume the world
   */
}

void Daemon::onQuickShutdownSignal(const xzero::UnixSignalInfo& info) {
  logNotice("Initiating quick shutdown. (requested via {} by UID {} PID {})",
            PosixSignals::toString(info.signal),
            info.uid.value_or(-1),
            info.pid.value_or(-1));

  terminate();
}

void Daemon::onGracefulShutdownSignal(const xzero::UnixSignalInfo& info) {
  logNotice("Initiating graceful shutdown. (requested via {} by UID {} PID {})",
            PosixSignals::toString(info.signal),
            info.uid.value_or(-1),
            info.pid.value_or(-1));

  /* 1. stop all listeners
   * 2. wait until all requests have been handled.
   * 3. orderly shutdown
   */

  stop();
}

} // namespace x0d
