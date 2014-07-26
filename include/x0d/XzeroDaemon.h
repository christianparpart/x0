// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/sysconfig.h>
#include <x0d/Api.h>
#include <base/Buffer.h>
#include <base/Severity.h>
#include <base/Library.h>
#include <flow/vm/NativeCallback.h>
#include <flow/vm/Runtime.h>
#include <flow/vm/Runner.h>
#include <string>
#include <unordered_map>
#include <system_error>

namespace xzero {
class HttpServer;
}

namespace flow {
class Unit;
class CallExpr;
}

namespace x0d {

class XzeroEventHandler;
class XzeroPlugin;
class XzeroCore;

class X0D_API XzeroDaemon : public flow::FlowVM::Runtime {
 private:
  friend class XzeroPlugin;
  friend class XzeroCore;

  /** concats a path with a filename and optionally inserts a path seperator if
   * path
   *  doesn't contain a trailing path seperator. */
  static inline std::string pathcat(const std::string& path,
                                    const std::string& filename) {
    if (!path.empty() && path[path.size() - 1] != '/') {
      return path + "/" + filename;
    } else {
      return path + filename;
    }
  }

 public:
  XzeroDaemon(int argc, char* argv[]);
  ~XzeroDaemon();

  static XzeroDaemon* instance() { return instance_; }

  int run();

  void reexec();

  xzero::HttpServer* server() const { return server_; }

  void log(base::Severity severity, const char* msg, ...);

  void cycleLogs();

  // plugin management
  std::string pluginDirectory() const;
  void setPluginDirectory(const std::string& value);

  XzeroPlugin* loadPlugin(const std::string& name, std::error_code& ec);
  template <typename T>
  T* loadPlugin(const std::string& name, std::error_code& ec);
  void unloadPlugin(const std::string& name);
  bool pluginLoaded(const std::string& name) const;
  std::vector<std::string> pluginsLoaded() const;

  XzeroPlugin* registerPlugin(XzeroPlugin* plugin);
  XzeroPlugin* unregisterPlugin(XzeroPlugin* plugin);

  XzeroCore& core() const;

  void addComponent(const std::string& value);

  bool setup(std::unique_ptr<std::istream>&& settings,
             const std::string& filename = std::string(),
             int optimizationLevel = 2);
  bool setup(const std::string& filename, int optimizationLevel = 2);

 public:  // FlowBackend overrides
  virtual bool import(const std::string& name, const std::string& path,
                      std::vector<flow::FlowVM::NativeCallback*>* builtins);

  // new
  template <typename... ArgTypes>
  flow::FlowVM::NativeCallback& setupFunction(
      const std::string& name, const flow::FlowVM::NativeCallback::Functor& cb,
      ArgTypes... argTypes);
  template <typename... ArgTypes>
  flow::FlowVM::NativeCallback& sharedFunction(
      const std::string& name, const flow::FlowVM::NativeCallback::Functor& cb,
      ArgTypes... argTypes);
  template <typename... ArgTypes>
  flow::FlowVM::NativeCallback& mainFunction(
      const std::string& name, const flow::FlowVM::NativeCallback::Functor& cb,
      ArgTypes... argTypes);
  template <typename... ArgTypes>
  flow::FlowVM::NativeCallback& mainHandler(
      const std::string& name, const flow::FlowVM::NativeCallback::Functor& cb,
      ArgTypes... argTypes);

 private:
  bool validateConfig();

  bool validate(const std::string& context,
                const std::vector<flow::CallExpr*>& calls,
                const std::vector<std::string>& api);

  bool createPidFile();
  bool parseCommandLineArgs();
  bool verifyEnv();
  bool setupConfig();
  void daemonize();
  bool drop_privileges(const std::string& username,
                       const std::string& groupname);

  void installCrashHandler();

 private:
  int argc_;
  char** argv_;
  bool showGreeter_;
  std::string configfile_;
  std::string pidfile_;
  std::string user_;
  std::string group_;

  std::string logTarget_;
  std::string logFile_;
  base::Severity logLevel_;

  base::Buffer instant_;
  std::string documentRoot_;

  int nofork_;
  int systemd_;
  int dumpAST_;
  int dumpIR_;
  int dumpTargetCode_;
  int optimizationLevel_;
  xzero::HttpServer* server_;
  unsigned evFlags_;
  XzeroEventHandler* eventHandler_;

  std::string pluginDirectory_;
  std::vector<XzeroPlugin*> plugins_;
  std::unordered_map<XzeroPlugin*, base::Library> pluginLibraries_;
  XzeroCore* core_;
  std::vector<std::string> components_;

  // flow configuration
  std::unique_ptr<flow::Unit> unit_;
  std::unique_ptr<flow::FlowVM::Program> program_;
  flow::FlowVM::Handler* main_;
  std::vector<std::string> setupApi_;
  std::vector<std::string> mainApi_;

  static XzeroDaemon* instance_;
};

// {{{ inlines
template <typename T>
inline T* XzeroDaemon::loadPlugin(const std::string& name,
                                  std::error_code& ec) {
  return dynamic_cast<T*>(loadPlugin(name, ec));
}

inline XzeroCore& XzeroDaemon::core() const { return *core_; }

template <typename... ArgTypes>
inline flow::FlowVM::NativeCallback& XzeroDaemon::setupFunction(
    const std::string& name, const flow::FlowVM::NativeCallback::Functor& cb,
    ArgTypes... argTypes) {
  setupApi_.push_back(name);
  return registerFunction(name, flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline flow::FlowVM::NativeCallback& XzeroDaemon::sharedFunction(
    const std::string& name, const flow::FlowVM::NativeCallback::Functor& cb,
    ArgTypes... argTypes) {
  setupApi_.push_back(name);
  mainApi_.push_back(name);
  return registerFunction(name, flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline flow::FlowVM::NativeCallback& XzeroDaemon::mainFunction(
    const std::string& name, const flow::FlowVM::NativeCallback::Functor& cb,
    ArgTypes... argTypes) {
  mainApi_.push_back(name);
  return registerFunction(name, flow::FlowType::Void).bind(cb).params(
      argTypes...);
}

template <typename... ArgTypes>
inline flow::FlowVM::NativeCallback& XzeroDaemon::mainHandler(
    const std::string& name, const flow::FlowVM::NativeCallback::Functor& cb,
    ArgTypes... argTypes) {
  mainApi_.push_back(name);
  return registerHandler(name).bind(cb).params(argTypes...);
}
// }}}

}  // namespace x0
