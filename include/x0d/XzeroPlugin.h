// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#ifndef x0_plugin_hpp
#define x0_plugin_hpp (1)

#include <x0d/Api.h>
#include <x0d/XzeroDaemon.h>
#include <xzero/HttpRequest.h>
#include <flow/AST.h>
#include <base/Severity.h>
#include <base/Defines.h>
#include <base/Types.h>

#include <string>
#include <vector>
#include <functional>
#include <system_error>

namespace xzero {
class HttpServer;
}

//! \addtogroup daemon
//@{

namespace x0d {

class XzeroDaemon;

/**
 * \brief base class for all plugins for use within this x0 web server.
 *
 * \see server, connection, request, response
 */
class X0D_API XzeroPlugin {
 private:
  XzeroPlugin(const XzeroPlugin&) = delete;
  XzeroPlugin& operator=(const XzeroPlugin&) = delete;

 public:
  XzeroPlugin(XzeroDaemon* daemon, const std::string& name);
  virtual ~XzeroPlugin();

  std::string name() const;

  virtual bool post_config();
  virtual bool post_check();
  virtual void cycleLogs();

  template <typename... Args>
  inline void log(base::Severity sv, const char* msg, Args&&... args);

  template <typename... Args>
  inline void debug(int level, const char* msg, Args&&... args);

#if !defined(XZERO_NDEBUG)
  inline int debug_level() const;
  void debug_level(int value);
#endif

  XzeroDaemon& daemon() const;
  xzero::HttpServer& server() const;

 protected:
  // flow configuration API
  template <typename Class, typename... ArgTypes>
  flow::FlowVM::NativeCallback& setupFunction(
      const std::string& name, void (Class::*method)(flow::FlowVM::Params&),
      ArgTypes... argTypes);
  template <typename Class, typename... ArgTypes>
  flow::FlowVM::NativeCallback& sharedFunction(
      const std::string& name,
      void (Class::*method)(xzero::HttpRequest* r, flow::FlowVM::Params&),
      ArgTypes... argTypes);
  template <typename Class, typename... ArgTypes>
  flow::FlowVM::NativeCallback& mainFunction(
      const std::string& name,
      void (Class::*method)(xzero::HttpRequest* r, flow::FlowVM::Params&),
      ArgTypes... argTypes);
  template <typename Class, typename... ArgTypes>
  flow::FlowVM::NativeCallback& mainHandler(
      const std::string& name,
      bool (Class::*method)(xzero::HttpRequest* r, flow::FlowVM::Params&),
      ArgTypes... argTypes);

  // hook setup API
  void onWorkerSpawn(std::function<void(xzero::HttpWorker*)>&& callback) {
    auto handle = server().onWorkerSpawn.connect(std::move(callback));
    cleanups_.push_back([=]() { server().onWorkerSpawn.disconnect(handle); });
  }

  void onWorkerUnspawn(std::function<void(xzero::HttpWorker*)>&& callback) {
    auto handle = server().onWorkerUnspawn.connect(std::move(callback));
    cleanups_.push_back([=]() { server().onWorkerUnspawn.disconnect(handle); });
  }

  void onConnectionOpen(std::function<void(xzero::HttpConnection*)>&& callback) {
    auto handle = server().onConnectionOpen.connect(std::move(callback));
    cleanups_.push_back([=]() {
      server().onConnectionOpen.disconnect(handle);
    });
  }

  void onConnectionStateChanged(std::function<
      void(xzero::HttpConnection*, xzero::HttpConnection::State)>&& callback) {
    auto handle =
        server().onConnectionStateChanged.connect(std::move(callback));
    cleanups_.push_back([=]() {
      server().onConnectionStateChanged.disconnect(handle);
    });
  }

  void onConnectionClose(std::function<void(xzero::HttpConnection*)>&& callback) {
    auto handle = server().onConnectionClose.connect(std::move(callback));
    cleanups_.push_back([=]() {
      server().onConnectionClose.disconnect(handle);
    });
  }

  void onPreProcess(std::function<void(xzero::HttpRequest*)>&& callback) {
    auto handle = server().onPreProcess.connect(std::move(callback));
    cleanups_.push_back([=]() { server().onPreProcess.disconnect(handle); });
  }

  void onPostProcess(std::function<void(xzero::HttpRequest*)>&& callback) {
    auto handle = server().onPostProcess.connect(std::move(callback));
    cleanups_.push_back([=]() { server().onPostProcess.disconnect(handle); });
  }

  void onRequestDone(std::function<void(xzero::HttpRequest*)>&& callback) {
    auto handle = server().onRequestDone.connect(std::move(callback));
    cleanups_.push_back([=]() { server().onRequestDone.disconnect(handle); });
  }

 private:
  flow::FlowVM::NativeCallback& addNative(flow::FlowVM::NativeCallback& cb);

 protected:
  XzeroDaemon* daemon_;
  xzero::HttpServer* server_;
  std::string name_;
  std::list<std::function<void()>> cleanups_;
  std::vector<flow::FlowVM::NativeCallback*> natives_;

#if !defined(XZERO_NDEBUG)
  int debugLevel_;
#endif

  friend class XzeroDaemon;
};

// {{{ flow integration
inline flow::FlowVM::NativeCallback& XzeroPlugin::addNative(
    flow::FlowVM::NativeCallback& cb) {
  natives_.push_back(&cb);
  return cb;
}

template <typename Class, typename... ArgTypes>
inline flow::FlowVM::NativeCallback& XzeroPlugin::setupFunction(
    const std::string& name, void (Class::*method)(flow::FlowVM::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->setupFunction(
      name, [=](flow::FlowVM::Params& args) { (((Class*)this)->*method)(args); },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline flow::FlowVM::NativeCallback& XzeroPlugin::sharedFunction(
    const std::string& name,
    void (Class::*method)(xzero::HttpRequest*, flow::FlowVM::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->sharedFunction(
      name, [=](flow::FlowVM::Params& args) {
              (((Class*)this)->*method)(
                  (xzero::HttpRequest*)args.caller()->userdata(), args);
            },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline flow::FlowVM::NativeCallback& XzeroPlugin::mainFunction(
    const std::string& name,
    void (Class::*method)(xzero::HttpRequest*, flow::FlowVM::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->mainFunction(
      name, [=](flow::FlowVM::Params& args) {
              (((Class*)this)->*method)(
                  (xzero::HttpRequest*)args.caller()->userdata(), args);
            },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline flow::FlowVM::NativeCallback& XzeroPlugin::mainHandler(
    const std::string& name,
    bool (Class::*method)(xzero::HttpRequest*, flow::FlowVM::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->mainHandler(
      name, [=](flow::FlowVM::Params& args) {
              args.setResult((((Class*)this)->*method)(
                  (xzero::HttpRequest*)args.caller()->userdata(), args));
            },
      argTypes...));
}
// }}}

// {{{ inlines
inline XzeroDaemon& XzeroPlugin::daemon() const { return *daemon_; }

inline xzero::HttpServer& XzeroPlugin::server() const { return *server_; }

/** retrieves the plugin's unique basename (index, userdir, sendfile, auth,
 * etc...) */
inline std::string XzeroPlugin::name() const { return name_; }

template <typename... Args>
inline void XzeroPlugin::log(base::Severity sv, const char* msg, Args&&... args) {
  base::Buffer fmt;
  fmt.push_back(name_);
  fmt.push_back(": ");
  fmt.push_back(msg);

  server_->log(sv, fmt.c_str(), args...);
}

template <typename... Args>
inline void XzeroPlugin::debug(int level, const char* msg, Args&&... args) {
#if !defined(XZERO_NDEBUG)
  if (level <= debugLevel_) {
    base::Buffer fmt;
    fmt.push_back(name_);
    fmt.push_back(": ");
    fmt.push_back(msg);

    server_->log(base::Severity::debug, fmt.c_str(), args...);
  }
#endif
}

#if !defined(XZERO_NDEBUG)
inline int XzeroPlugin::debug_level() const { return debugLevel_; }

inline void XzeroPlugin::debug_level(int value) { debugLevel_ = value; }
#endif
// }}}

#define X0D_EXPORT_PLUGIN(pluginName) \
  X0D_EXPORT_PLUGIN_CLASS(pluginName##_plugin)

#define X0D_EXPORT_PLUGIN_CLASS(className)                 \
  extern "C" BASE_EXPORT x0d::XzeroPlugin* x0plugin_init( \
      x0d::XzeroDaemon* d, const std::string& name) {     \
    return new className(d, name);                        \
  }

}  // namespace x0d

//@}

#endif
