// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/XzeroDaemon.h>
#include <x0d/XzeroContext.h>
#include <xzero/Buffer.h>
#include <xzero-flow/vm/Params.h>
#include <string>
#include <list>
#include <vector>
#include <functional>

namespace xzero {
  class Server;
  class Connection;

  namespace http {
    class HttpRequest;
    class HttpResponse;
  }

  namespace flow {
    namespace vm {
      class NativeCallback;
      class Params;
    }
  }
}

namespace x0d {

class XzeroDaemon;

class XzeroModule {
 public:
  XzeroModule(XzeroDaemon* x0d, const std::string& name);
  virtual ~XzeroModule();

  const std::string& name() const;
  XzeroDaemon& daemon() const;

 protected:
  using Params = xzero::flow::vm::Params;
  using NativeCallback = xzero::flow::vm::NativeCallback;
  using FlowType = xzero::flow::FlowType;

  // flow configuration API
  template <typename Class, typename... ArgTypes>
  NativeCallback& setupFunction(
      const std::string& name, void (Class::*method)(Params&),
      ArgTypes... argTypes);

  template <typename Class, typename... ArgTypes>
  NativeCallback& sharedFunction(
      const std::string& name,
      void (Class::*method)(XzeroContext*, Params&),
      ArgTypes... argTypes);

  template <typename Class, typename... ArgTypes>
  NativeCallback& sharedFunction(
      const std::string& name,
      void (Class::*setupCall)(Params&),
      void (Class::*mainCall)(XzeroContext*, Params&),
      ArgTypes... argTypes);

  template <typename Class, typename... ArgTypes>
  NativeCallback& mainFunction(
      const std::string& name,
      void (Class::*method)(XzeroContext*, Params&),
      ArgTypes... argTypes);

  template <typename Class, typename... ArgTypes>
  NativeCallback& mainHandler(
      const std::string& name,
      bool (Class::*method)(XzeroContext*, Params&),
      ArgTypes... argTypes);

  virtual void onPostConfig();

  void onCycleLogs(std::function<void()> callback);
  void onConnectionOpen(std::function<void(xzero::Connection*)> callback);
  void onConnectionClose(std::function<void(xzero::Connection*)> callback);
  void onPreProcess(std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> callback);
  void onPostProcess(std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> callback);
  void onRequestDone(std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> callback);

 private:
  xzero::flow::vm::NativeCallback& addNative(xzero::flow::vm::NativeCallback& cb);

 protected:
  XzeroDaemon* daemon_;
  std::string name_;
  std::list<std::function<void()>> cleanups_;
  std::vector<xzero::flow::vm::NativeCallback*> natives_;

  friend class XzeroDaemon;
};

// {{{ inlines
inline const std::string& XzeroModule::name() const {
  return name_;
}

inline XzeroDaemon& XzeroModule::daemon() const {
  return *daemon_;
}
// }}}
// {{{ flow integration
inline xzero::flow::vm::NativeCallback& XzeroModule::addNative(
    xzero::flow::vm::NativeCallback& cb) {
  natives_.push_back(&cb);
  return cb;
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroModule::setupFunction(
    const std::string& name, void (Class::*method)(xzero::flow::vm::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->setupFunction(
      name, [=](xzero::flow::vm::Params& args) { (((Class*)this)->*method)(args); },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroModule::sharedFunction(
    const std::string& name,
    void (Class::*method)(XzeroContext*, xzero::flow::vm::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->sharedFunction(
      name,
      [=](xzero::flow::vm::Params& args) {
        auto cx = (XzeroContext*) args.caller()->userdata();
        (((Class*)this)->*method)(cx, args);
      },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroModule::sharedFunction(
    const std::string& name,
    void (Class::*setupCall)(xzero::flow::vm::Params&),
    void (Class::*mainCall)(XzeroContext*, xzero::flow::vm::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->sharedFunction(
      name,
      [=](xzero::flow::vm::Params& args) {
        auto cx = (XzeroContext*) args.caller()->userdata();
        if (cx) {
          (((Class*)this)->*mainCall)(cx, args);
        } else {
          (((Class*)this)->*setupCall)(args);
        }
      },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroModule::mainFunction(
    const std::string& name,
    void (Class::*method)(XzeroContext*, xzero::flow::vm::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->mainFunction(
      name,
      [=](xzero::flow::vm::Params& args) {
        auto cx = (XzeroContext*) args.caller()->userdata();
        (((Class*)this)->*method)(cx, args);
      },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::vm::NativeCallback& XzeroModule::mainHandler(
    const std::string& name,
    bool (Class::*method)(XzeroContext*, xzero::flow::vm::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->mainHandler(
      name,
      [=](xzero::flow::vm::Params& args) {
        auto cx = (XzeroContext*) args.caller()->userdata();
        args.setResult((((Class*)this)->*method)(cx, args));
      },
      argTypes...));
}
// }}}

#define X0D_EXPORT_MODULE(moduleName)                                          \
  X0D_EXPORT_MODULE_CLASS(moduleName##_module)

#define X0D_EXPORT_MODULE_CLASS(className)                                     \
  extern "C" BASE_EXPORT x0d::XzeroModule* x0module_init(                      \
      x0d::XzeroDaemon* d, const std::string& name) {                          \
    return new className(d, name);                                             \
  }

} // namespace x0d
