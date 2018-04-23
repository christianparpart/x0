// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/Config.h>

#include <xzero/Buffer.h>
#include <xzero/MimeTypes.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/Callback.h>
#include <xzero/UnixTime.h>
#include <xzero/Duration.h>
#include <xzero/net/TcpConnector.h>
#include <xzero/executor/ThreadedExecutor.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/http/HttpFileHandler.h>
#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero-flow/AST.h>
#include <xzero-flow/NativeCallback.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/vm/Runtime.h>
#include <xzero-flow/vm/Program.h>
#include <xzero-flow/vm/Handler.h>
#include <list>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <iosfwd>

namespace xzero {
  class IPAddress;
  class Connection;
  class Connector;
  class EventLoop;
  struct UnixSignalInfo;

  namespace http {
    class HttpRequest;
    class HttpResponse;
  }

  namespace flow {
    class CallExpr;
    class IRGenerator;
  }
}

namespace x0d {

class Module;
class SignalHandler;

class ConfigurationError : public std::runtime_error {
 public:
  explicit ConfigurationError(const std::string& diagnostics)
      : std::runtime_error("Configuration error. " + diagnostics) {}
};

// TODO: probably call me ServiceState
enum class DaemonState {
  Inactive,
  Initializing,
  Running,
  Upgrading,
  GracefullyShuttingdown
};

class Daemon : public xzero::flow::Runtime {
 public:
  typedef xzero::Callback<void(xzero::Connection*)> ConnectionHook;
  typedef xzero::Callback<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> RequestHook;
  typedef xzero::Callback<void()> CycleLogsHook;

 public:
  Daemon();
  ~Daemon();

  void setOptimizationLevel(int level) { optimizationLevel_ = level; }

  xzero::EventLoop* mainEventLoop() const { return eventLoops_[0].get(); }

  // {{{ config management
  std::unique_ptr<xzero::flow::Program> loadConfigFile(
      const std::string& configFileName);
  std::unique_ptr<xzero::flow::Program> loadConfigFile(
      const std::string& configFileName,
      bool printAST, bool printIR, bool printTC);
  std::unique_ptr<xzero::flow::Program> loadConfigEasy(
      const std::string& docroot, int port);
  std::unique_ptr<xzero::flow::Program> loadConfigEasy(
      const std::string& docroot, int port,
      bool printAST, bool printIR, bool printTC);
  std::unique_ptr<xzero::flow::Program> loadConfigStream(
      std::unique_ptr<std::istream>&& is, const std::string& name,
      bool printAST, bool printIR, bool printTC);
  void reloadConfiguration();
  void applyConfiguration(std::unique_ptr<xzero::flow::Program>&& program);
  // }}}

  void run();
  void terminate();

  xzero::Executor* selectClientExecutor();

  template<typename T>
  void setupConnector(const xzero::IPAddress& ipaddr, int port,
                      int backlog, int multiAccept,
                      bool reuseAddr, bool deferAccept, bool reusePort,
                      std::function<void(T*)> connectorVisitor);

  template<typename T>
  T* doSetupConnector(xzero::Executor* executor,
                      xzero::TcpConnector::ExecutorSelector clientExecutorSelector,
                      const xzero::IPAddress& ipaddr, int port,
                      int backlog, int multiAccept,
                      bool reuseAddr, bool deferAccept, bool reusePort);

  template <typename... ArgTypes>
  xzero::flow::NativeCallback& setupFunction(
      const std::string& name, xzero::flow::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::NativeCallback& sharedFunction(
      const std::string& name, xzero::flow::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::NativeCallback& mainFunction(
      const std::string& name, xzero::flow::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::NativeCallback& mainHandler(
      const std::string& name, xzero::flow::NativeCallback::Functor cb,
      ArgTypes... argTypes);

 public:
  // flow::Runtime overrides
  virtual bool import(
      const std::string& name,
      const std::string& path,
      std::vector<xzero::flow::NativeCallback*>* builtins);

 private:
  void validateConfig(xzero::flow::UnitSym* unit);
  void validateContext(const std::string& entrypointHandlerName,
                       const std::vector<std::string>& api,
                       xzero::flow::UnitSym* unit);
  void stopThreads();
  void startThreads();

  std::function<void()> createHandler(xzero::http::HttpRequest* request, xzero::http::HttpResponse* response);
  void handleRequest(xzero::http::HttpRequest* request, xzero::http::HttpResponse* response);

 public: // signals raised on request in order
  //! This hook is invoked once a new client has connected.
  ConnectionHook onConnectionOpen;

  //! is called at the very beginning of a request.
  RequestHook onPreProcess;

  //! gets invoked right before serializing headers.
  RequestHook onPostProcess;

  //! this hook is invoked once the request has been fully served to the client.
  RequestHook onRequestDone;

  //! Hook that is invoked when a connection gets closed.
  ConnectionHook onConnectionClose;

  //! Hook that is invoked whenever a cycle-the-logfiles is being triggered.
  CycleLogsHook onCycleLogs;

  xzero::MimeTypes& mimetypes() noexcept { return mimetypes_; }
  xzero::LocalFileRepository& vfs() noexcept { return vfs_; }
  xzero::http::HttpFileHandler& fileHandler() noexcept { return fileHandler_; }

  Config& config() const noexcept { return *config_; }

  template<typename T>
  T* loadModule();

  void start();
  void stop();

  /**
   * Removes all TcpConnector instances from this server.
   */
  void removeAllConnectors();

 private:
  std::unique_ptr<Config> createDefaultConfig();
  void patchProgramIR(xzero::flow::IRProgram* program,
                      xzero::flow::IRGenerator* irgen);
  void postConfig();
  std::unique_ptr<xzero::EventLoop> createEventLoop();
  void runOneThread(size_t index);
  void setThreadAffinity(int cpu, int workerId);

  DaemonState state() const { return state_; }
  void setState(DaemonState newState);

  void onConfigReloadSignal(const xzero::UnixSignalInfo& info);
  void onCycleLogsSignal(const xzero::UnixSignalInfo& info);
  void onUpgradeBinarySignal(const xzero::UnixSignalInfo& info);
  void onQuickShutdownSignal(const xzero::UnixSignalInfo& info);
  void onGracefulShutdownSignal(const xzero::UnixSignalInfo& info);

 private:
  unsigned generation_;                  //!< process generation number
  xzero::UnixTime startupTime_;          //!< process startup time
  std::atomic<bool> terminate_;

  xzero::MimeTypes mimetypes_;
  xzero::LocalFileRepository vfs_;

  size_t lastWorker_;                         //!< offset to the last elected worker
  xzero::ThreadedExecutor threadedExecutor_;  //!< non-main worker executor
  std::vector<std::unique_ptr<xzero::EventLoop>> eventLoops_; //!< one for each thread
  std::list<std::unique_ptr<Module>> modules_; //!< list of loaded modules
  std::list<std::unique_ptr<xzero::TcpConnector>> connectors_; //!< TCP (HTTP) connectors

  // Flow configuration
  std::unique_ptr<xzero::flow::Program> program_; // kept to preserve strong reference count
  xzero::flow::Handler* main_;
  std::vector<std::string> setupApi_;
  std::vector<std::string> mainApi_;
  int optimizationLevel_;

  // HTTP
  xzero::http::HttpFileHandler fileHandler_;
  std::shared_ptr<xzero::http::http1::ConnectionFactory> http1_;

  // setup phase
  std::string configFilePath_;
  std::unique_ptr<Config> config_;

  // signal handling
  DaemonState state_;

  friend class CoreModule;
};

// {{{ inlines
template <typename... ArgTypes>
inline xzero::flow::NativeCallback& Daemon::setupFunction(
    const std::string& name, xzero::flow::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  setupApi_.push_back(name);
  return registerFunction(name, xzero::flow::LiteralType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::NativeCallback& Daemon::sharedFunction(
    const std::string& name, xzero::flow::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  setupApi_.push_back(name);
  mainApi_.push_back(name);
  return registerFunction(name, xzero::flow::LiteralType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::NativeCallback& Daemon::mainFunction(
    const std::string& name, xzero::flow::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  mainApi_.push_back(name);
  return registerFunction(name, xzero::flow::LiteralType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::NativeCallback& Daemon::mainHandler(
    const std::string& name, xzero::flow::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  mainApi_.push_back(name);
  return registerHandler(name).bind(cb).params(argTypes...);
}

template<typename T>
inline T* Daemon::loadModule() {
  modules_.emplace_back(std::make_unique<T>(this));
  return static_cast<T*>(modules_.back().get());
}

// }}}

} // namespace x0d
