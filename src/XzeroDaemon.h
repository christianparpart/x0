// This file is part of the "x0" project
//   (c) 2009-2015 Christian Parpart <https://github.com/christianparpart>
//
// x0 is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License v3.0.
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
#pragma once

#include "Config.h"

#include <xzero/net/Server.h>
#include <xzero/Buffer.h>
#include <xzero/MimeTypes.h>
#include <xzero/io/LocalFileRepository.h>
#include <xzero/Signal.h>
#include <xzero/UnixTime.h>
#include <xzero/Duration.h>
#include <xzero/net/InetConnector.h>
#include <xzero/executor/ThreadedExecutor.h>
#include <xzero/executor/NativeScheduler.h>
#include <xzero/http/http1/ConnectionFactory.h>
#include <xzero-flow/AST.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/vm/Runtime.h>
#include <xzero-flow/vm/Program.h>
#include <xzero-flow/vm/Handler.h>
#include <xzero-flow/vm/NativeCallback.h>
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
  class Scheduler;

  namespace http {
    class HttpRequest;
    class HttpResponse;
  }

  namespace flow {
    class CallExpr;
  }
}

namespace x0d {

class XzeroModule;

class XzeroDaemon : public xzero::flow::vm::Runtime {
 public:
  typedef xzero::Signal<void(xzero::Connection*)> ConnectionHook;
  typedef xzero::Signal<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> RequestHook;
  typedef xzero::Signal<void()> CycleLogsHook;

 public:
  XzeroDaemon();
  ~XzeroDaemon();

  void setOptimizationLevel(int level) { optimizationLevel_ = level; }

  void loadConfigFile(const std::string& configFileName);
  void loadConfigEasy(const std::string& docroot, int port);
  void loadConfigStream(std::unique_ptr<std::istream>&& is, const std::string& name);
  bool configure();

  void run();
  void terminate();

  xzero::flow::Unit* programAST() const noexcept { return unit_.get(); }
  xzero::flow::vm::Program* program() const noexcept { return program_.get(); }
  xzero::flow::IRProgram* programIR() const noexcept { return programIR_.get(); }

  xzero::Scheduler* selectClientScheduler();

  template<typename T>
  void setupConnector(const xzero::IPAddress& ipaddr, int port,
                      int backlog, int multiAccept,
                      bool reuseAddr, bool reusePort,
                      std::function<void(T*)> connectorVisitor);

  template<typename T>
  T* doSetupConnector(xzero::Scheduler* listenerScheduler,
                      xzero::InetConnector::SchedulerSelector clientSchedulerSelector,
                      const xzero::IPAddress& ipaddr, int port,
                      int backlog, int multiAccept,
                      bool reuseAddr, bool reusePort);

  template <typename... ArgTypes>
  xzero::flow::vm::NativeCallback& setupFunction(
      const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::vm::NativeCallback& sharedFunction(
      const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::vm::NativeCallback& mainFunction(
      const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
      ArgTypes... argTypes);

  template <typename... ArgTypes>
  xzero::flow::vm::NativeCallback& mainHandler(
      const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
      ArgTypes... argTypes);

 public:
  // flow::vm::Runtime overrides
  virtual bool import(
      const std::string& name,
      const std::string& path,
      std::vector<xzero::flow::vm::NativeCallback*>* builtins);

 private:
  void validateConfig();
  void validateContext(const std::string& entrypointHandlerName,
                       const std::vector<std::string>& api);

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

  template<typename T>
  T* loadModule();

  void postConfig();

  void runOneThread(xzero::Scheduler* scheduler);

  std::unique_ptr<xzero::Scheduler> newScheduler();

 private:
  unsigned generation_;                  //!< process generation number
  xzero::UnixTime startupTime_;          //!< process startup time
  std::atomic<bool> terminate_;

  xzero::MimeTypes mimetypes_;
  xzero::LocalFileRepository vfs_;

  off_t lastWorker_;                          //!< offset to the last elected worker
  xzero::ThreadedExecutor threadedExecutor_;  //!< non-main worker executor
  std::vector<std::unique_ptr<xzero::Scheduler>> schedulers_; //!< schedulers, one for each thread
  std::list<XzeroModule*> modules_;           //!< list of loaded modules
  std::unique_ptr<xzero::Server> server_;     //!< (HTTP) server instance

  // Flow configuration
  std::unique_ptr<xzero::flow::Unit> unit_;
  std::unique_ptr<xzero::flow::IRProgram> programIR_;
  std::unique_ptr<xzero::flow::vm::Program> program_;
  xzero::flow::vm::Handler* main_;
  std::vector<std::string> setupApi_;
  std::vector<std::string> mainApi_;
  int optimizationLevel_;

  // HTTP
  std::shared_ptr<xzero::http::http1::ConnectionFactory> http1_;

  // setup phase
  std::unique_ptr<Config> config_;

  friend class CoreModule;
};

// {{{ inlines
template <typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroDaemon::setupFunction(
    const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  setupApi_.push_back(name);
  return registerFunction(name, xzero::flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroDaemon::sharedFunction(
    const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  setupApi_.push_back(name);
  mainApi_.push_back(name);
  return registerFunction(name, xzero::flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroDaemon::mainFunction(
    const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  mainApi_.push_back(name);
  return registerFunction(name, xzero::flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroDaemon::mainHandler(
    const std::string& name, xzero::flow::vm::NativeCallback::Functor cb,
    ArgTypes... argTypes) {
  mainApi_.push_back(name);
  return registerHandler(name).bind(cb).params(argTypes...);
}

template<typename T>
inline T* XzeroDaemon::loadModule() {
  modules_.emplace_back(new T(this));
  return static_cast<T*>(modules_.back());
}

// }}}

} // namespace x0d
