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
#include <xzero/DateTime.h>
#include <xzero/TimeSpan.h>
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
class XzeroWorker;

class XzeroDaemon : public xzero::flow::vm::Runtime {
 public:
  typedef xzero::Signal<void(xzero::Connection*)> ConnectionHook;
  typedef xzero::Signal<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> RequestHook;
  typedef xzero::Signal<void(XzeroWorker*)> WorkerHook;
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

  template<typename T>
  T* setupConnector(const xzero::IPAddress& ipaddr, int port,
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

  XzeroWorker* mainWorker() const;

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

  //! Hook that is invoked whenever a worker is being spawned.
  WorkerHook onWorkerSpawn;

  //! Hook that is invoked whenever a worker is being despawned.
  WorkerHook onWorkerUnspawn;

  xzero::MimeTypes& mimetypes() noexcept { return mimetypes_; }
  xzero::LocalFileRepository& vfs() noexcept { return vfs_; }

  template<typename T>
  T* loadModule();

  void postConfig();

 private:
  unsigned generation_;                   //!< process generation number
  xzero::DateTime startupTime_;          //!< process startup time

  xzero::MimeTypes mimetypes_;
  xzero::LocalFileRepository vfs_;

  off_t lastWorker_;                      //!< offset to the last elected worker
  std::vector<XzeroWorker*> workers_;     //!< list of workers
  std::list<XzeroModule*> modules_;       //!< list of loaded modules
  std::unique_ptr<xzero::Server> server_;//!< (HTTP) server instance

  // Flow configuration
  std::unique_ptr<xzero::flow::Unit> unit_;
  std::unique_ptr<xzero::flow::IRProgram> programIR_;
  std::unique_ptr<xzero::flow::vm::Program> program_;
  xzero::flow::vm::Handler* main_;
  std::vector<std::string> setupApi_;
  std::vector<std::string> mainApi_;
  int optimizationLevel_;

  // HTTP
  bool advertise_; //!< whether or not to provide Server response header
  size_t maxRequestHeaderSize_;
  size_t maxRequestHeaderCount_;
  size_t maxRequestBodySize_;
  size_t requestHeaderBufferSize_;
  size_t requestBodyBufferSize_;
  std::shared_ptr<xzero::http::http1::ConnectionFactory> http1_;

  // networking
  bool tcpCork_;
  bool tcpNoDelay_;
  size_t maxConnections_;
  xzero::TimeSpan maxReadIdle_;
  xzero::TimeSpan maxWriteIdle_;
  xzero::TimeSpan tcpFinTimeout_;
  xzero::TimeSpan lingering_;


  // setup phase
  std::unique_ptr<Config> config_;

  friend class CoreModule;
};

// {{{ inlines
inline XzeroWorker* XzeroDaemon::mainWorker() const {
  return workers_.front();
}

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
