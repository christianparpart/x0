// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/LiteralType.h>
#include <xzero-flow/Signature.h>
#include <xzero-flow/util/RegExp.h>
#include <xzero-flow/vm/Runner.h>       // Runner*, Runner::Value
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>
#include <string>
#include <vector>
#include <functional>
#include <variant>
#include <optional>

namespace xzero::flow {

class Params;
class Instr;
class IRBuilder;
class Runtime;

enum class Attribute : unsigned {
  Experimental    = 0x0001, // implementation is experimental, hence, parser can warn on use
  NoReturn        = 0x0002, // implementation never returns to program code
  SideEffectFree  = 0x0004, // implementation is side effect free
};

class NativeCallback {
 public:
  using Functor = std::function<void(Params& args)>;
  using Verifier = std::function<bool(Instr*, IRBuilder*)>;
  using DefaultValue = std::variant<std::monostate,
        bool, FlowString, FlowNumber, IPAddress, Cidr, util::RegExp>;

 private:
  Runtime* runtime_;
  bool isHandler_;
  Verifier verifier_;
  Functor function_;
  Signature signature_;

  // function attributes
  unsigned attributes_;

  // following attribs are irrelevant to the VM but useful for the frontend
  std::vector<std::string> names_;
  std::vector<DefaultValue> defaults_;

 public:
  NativeCallback(Runtime* runtime, const std::string& _name);
  NativeCallback(Runtime* runtime, const std::string& _name,
                 LiteralType _returnType);
  ~NativeCallback();

  bool isHandler() const noexcept;
  bool isFunction() const noexcept;
  const std::string name() const noexcept;
  const Signature& signature() const noexcept;

  // signature builder

  /** Declare the return type. */
  NativeCallback& returnType(LiteralType type);

  /** Declare a single named parameter without default value. */
  template <typename T>
  NativeCallback& param(const std::string& name);

  /** Declare a single named parameter with default value. */
  template <typename T>
  NativeCallback& param(const std::string& name, T defaultValue);

  /** Declare ordered parameter signature. */
  template <typename... Args>
  NativeCallback& params(Args... args);

  // semantic verifier
  NativeCallback& verifier(const Verifier& vf);
  template <typename Class>
  NativeCallback& verifier(bool (Class::*method)(Instr*, IRBuilder*), Class* obj);
  template <typename Class>
  NativeCallback& verifier(bool (Class::*method)(Instr*, IRBuilder*));
  bool verify(Instr* call, IRBuilder* irBuilder);

  // bind callback
  NativeCallback& bind(const Functor& cb);
  template <typename Class>
  NativeCallback& bind(void (Class::*method)(Params&), Class* obj);
  template <typename Class>
  NativeCallback& bind(void (Class::*method)(Params&));

  // named parameter handling
  bool parametersNamed() const;
  const std::string& getParamNameAt(size_t i) const;
  const DefaultValue& getDefaultParamAt(size_t i) const;
  int findParamByName(const std::string& name) const;

  // attributes
  NativeCallback& setNoReturn() noexcept;
  NativeCallback& setReadOnly() noexcept;
  NativeCallback& setExperimental() noexcept;

  bool getAttribute(Attribute t) const noexcept { return attributes_ & unsigned(t); };
  bool isNeverReturning() const noexcept { return getAttribute(Attribute::NoReturn); }
  bool isReadOnly() const noexcept { return getAttribute(Attribute::SideEffectFree); }
  bool isExperimental() const noexcept { return getAttribute(Attribute::Experimental); }

  // runtime
  void invoke(Params& args) const;
};

// {{{ inlines
inline NativeCallback& NativeCallback::returnType(LiteralType type) {
  signature_.setReturnType(type);
  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<bool>(const std::string& name) {
  signature_.args().push_back(LiteralType::Boolean);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<bool>(const std::string& name,
                                                   bool defaultValue) {
  signature_.args().push_back(LiteralType::Boolean);
  names_.push_back(name);
  defaults_.push_back(defaultValue);

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<FlowNumber>(
    const std::string& name) {
  signature_.args().push_back(LiteralType::Number);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<FlowNumber>(
    const std::string& name, FlowNumber defaultValue) {
  signature_.args().push_back(LiteralType::Number);
  names_.push_back(name);
  defaults_.push_back(defaultValue);

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<int>(const std::string& name) {
  signature_.args().push_back(LiteralType::Number);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<int>(const std::string& name,
                                                  int defaultValue) {
  signature_.args().push_back(LiteralType::Number);
  names_.push_back(name);
  defaults_.push_back(FlowNumber{defaultValue});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<FlowString>(
    const std::string& name) {
  signature_.args().push_back(LiteralType::String);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<FlowString>(
    const std::string& name, FlowString defaultValue) {
  signature_.args().push_back(LiteralType::String);
  names_.push_back(name);
  defaults_.push_back(defaultValue);

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<IPAddress>(
    const std::string& name) {
  signature_.args().push_back(LiteralType::IPAddress);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<IPAddress>(
    const std::string& name, IPAddress defaultValue) {
  signature_.args().push_back(LiteralType::IPAddress);
  names_.push_back(name);
  defaults_.push_back(defaultValue);

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<Cidr>(const std::string& name) {
  signature_.args().push_back(LiteralType::Cidr);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<Cidr>(const std::string& name,
                                                   Cidr defaultValue) {
  signature_.args().push_back(LiteralType::Cidr);
  names_.push_back(name);
  defaults_.push_back(defaultValue);

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<util::RegExp>(const std::string& name) {
  signature_.args().push_back(LiteralType::RegExp);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<util::RegExp>(const std::string& name,
                                                           util::RegExp defaultValue) {
  signature_.args().push_back(LiteralType::RegExp);
  names_.push_back(name);
  defaults_.push_back(defaultValue);

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<FlowIntArray>(
    const std::string& name) {
  assert(defaults_.size() == names_.size());

  signature_.args().push_back(LiteralType::IntArray);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

template <>
inline NativeCallback& NativeCallback::param<FlowStringArray>(
    const std::string& name) {
  assert(defaults_.size() == names_.size());

  signature_.args().push_back(LiteralType::StringArray);
  names_.push_back(name);
  defaults_.push_back(std::monostate{});

  return *this;
}

// ------------------------------------------------------------------------------------------

template <typename... Args>
inline NativeCallback& NativeCallback::params(Args... args) {
  signature_.setArgs({args...});
  return *this;
}

inline NativeCallback& NativeCallback::verifier(const Verifier& vf) {
  verifier_ = vf;
  return *this;
}

template <typename Class>
inline NativeCallback& NativeCallback::verifier(
    bool (Class::*method)(Instr*, IRBuilder*),
    Class* obj) {
  verifier_ = std::bind(method, obj, std::placeholders::_1,
                                     std::placeholders::_2);
  return *this;
}

template <typename Class>
inline NativeCallback& NativeCallback::verifier(bool (Class::*method)(Instr*, IRBuilder*)) {
  verifier_ =
      std::bind(method, static_cast<Class*>(runtime_), std::placeholders::_1,
                                                       std::placeholders::_2);
  return *this;
}

inline bool NativeCallback::verify(Instr* call, IRBuilder* irBuilder) {
  if (!verifier_)
    return true;

  return verifier_(call, irBuilder);
}

inline NativeCallback& NativeCallback::bind(const Functor& cb) {
  function_ = cb;
  return *this;
}

template <typename Class>
inline NativeCallback& NativeCallback::bind(void (Class::*method)(Params&),
                                            Class* obj) {
  function_ = std::bind(method, obj, std::placeholders::_1);
  return *this;
}

template <typename Class>
inline NativeCallback& NativeCallback::bind(void (Class::*method)(Params&)) {
  function_ =
      std::bind(method, static_cast<Class*>(runtime_), std::placeholders::_1,
                                                       std::placeholders::_2);
  return *this;
}

inline bool NativeCallback::parametersNamed() const { return !names_.empty(); }

inline const std::string& NativeCallback::getParamNameAt(size_t i) const {
  assert(i < names_.size());
  return names_[i];
}

inline const NativeCallback::DefaultValue& NativeCallback::getDefaultParamAt(size_t i) const {
  assert(i < defaults_.size());
  return defaults_[i];
}
// }}}

}  // namespace xzero::flow
