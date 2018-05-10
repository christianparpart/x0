// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0d/Daemon.h>
#include <x0d/Context.h>
#include <xzero/Buffer.h>
#include <flow/Params.h>
#include <string>
#include <list>
#include <vector>
#include <functional>

namespace xzero {
  class Connection;

  namespace http {
    class HttpRequest;
    class HttpResponse;
  }

  namespace flow {
    class NativeCallback;
  }
}

namespace x0d {

class Daemon;

class Module {
 public:
  Module(Daemon* x0d, const std::string& name);
  virtual ~Module();

  const std::string& name() const;
  Daemon& daemon() const;

 protected:
  using Params = xzero::flow::Params;
  using NativeCallback = xzero::flow::NativeCallback;
  using LiteralType = xzero::flow::LiteralType;

  // flow configuration API
  template <typename Class, typename... ArgTypes>
  NativeCallback& setupFunction(
      const std::string& name, void (Class::*method)(Params&),
      ArgTypes... argTypes);

  template <typename Class, typename... ArgTypes>
  NativeCallback& sharedFunction(
      const std::string& name,
      void (Class::*method)(Context*, Params&),
      ArgTypes... argTypes);

  template <typename Class, typename... ArgTypes>
  NativeCallback& sharedFunction(
      const std::string& name,
      void (Class::*setupCall)(Params&),
      void (Class::*mainCall)(Context*, Params&),
      ArgTypes... argTypes);

  template <typename Class, typename... ArgTypes>
  NativeCallback& mainFunction(
      const std::string& name,
      void (Class::*method)(Context*, Params&),
      ArgTypes... argTypes);

  template <typename Class, typename... ArgTypes>
  NativeCallback& mainHandler(
      const std::string& name,
      bool (Class::*method)(Context*, Params&),
      ArgTypes... argTypes);

  virtual void onPostConfig();

  void onCycleLogs(std::function<void()> callback);
  void onConnectionOpen(std::function<void(xzero::Connection*)> callback);
  void onConnectionClose(std::function<void(xzero::Connection*)> callback);
  void onPreProcess(std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> callback);
  void onPostProcess(std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> callback);
  void onRequestDone(std::function<void(xzero::http::HttpRequest*, xzero::http::HttpResponse*)> callback);

 private:
  xzero::flow::NativeCallback& addNative(xzero::flow::NativeCallback& cb);

 protected:
  Daemon* daemon_;
  std::string name_;
  std::list<std::function<void()>> cleanups_;
  std::vector<xzero::flow::NativeCallback*> natives_;

  friend class Daemon;
};

// {{{ inlines
inline const std::string& Module::name() const {
  return name_;
}

inline Daemon& Module::daemon() const {
  return *daemon_;
}
// }}}
// {{{ flow integration
inline xzero::flow::NativeCallback& Module::addNative(
    xzero::flow::NativeCallback& cb) {
  natives_.push_back(&cb);
  return cb;
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::NativeCallback& Module::setupFunction(
    const std::string& name, void (Class::*method)(xzero::flow::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->setupFunction(
      name, [=](xzero::flow::Params& args) { (((Class*)this)->*method)(args); },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::NativeCallback& Module::sharedFunction(
    const std::string& name,
    void (Class::*method)(Context*, xzero::flow::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->sharedFunction(
      name,
      [=](xzero::flow::Params& args) {
        auto cx = (Context*) args.caller()->userdata();
        (((Class*)this)->*method)(cx, args);
      },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::NativeCallback& Module::sharedFunction(
    const std::string& name,
    void (Class::*setupCall)(xzero::flow::Params&),
    void (Class::*mainCall)(Context*, xzero::flow::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->sharedFunction(
      name,
      [=](xzero::flow::Params& args) {
        auto cx = (Context*) args.caller()->userdata();
        if (cx) {
          (((Class*)this)->*mainCall)(cx, args);
        } else {
          (((Class*)this)->*setupCall)(args);
        }
      },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::NativeCallback& Module::mainFunction(
    const std::string& name,
    void (Class::*method)(Context*, xzero::flow::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->mainFunction(
      name,
      [=](xzero::flow::Params& args) {
        auto cx = (Context*) args.caller()->userdata();
        (((Class*)this)->*method)(cx, args);
      },
      argTypes...));
}

template <typename Class, typename... ArgTypes>
inline xzero::flow::NativeCallback& Module::mainHandler(
    const std::string& name,
    bool (Class::*method)(Context*, xzero::flow::Params&),
    ArgTypes... argTypes) {
  return addNative(daemon_->mainHandler(
      name,
      [=](xzero::flow::Params& args) {
        auto cx = (Context*) args.caller()->userdata();
        args.setResult((((Class*)this)->*method)(cx, args));
      },
      argTypes...));
}
// }}}

#define X0D_EXPORT_MODULE(moduleName)                                          \
  X0D_EXPORT_MODULE_CLASS(moduleName##_module)

#define X0D_EXPORT_MODULE_CLASS(className)                                     \
  extern "C" XZERO_EXPORT std::unique_ptr<x0d::Module> x0module_init(     \
      x0d::Daemon* d, const std::string& name) {                          \
    return std::make_unique<className>(d, name);                               \
  }

} // namespace x0d
