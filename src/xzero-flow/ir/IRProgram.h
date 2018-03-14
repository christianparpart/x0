// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero-flow/ir/Value.h>
#include <xzero-flow/ir/ConstantValue.h>
#include <xzero-flow/ir/IRBuiltinHandler.h>
#include <xzero-flow/ir/IRBuiltinFunction.h>
#include <xzero-flow/ir/Instructions.h>
#include <xzero-flow/ir/IRHandler.h>
#include <xzero-flow/Signature.h>
#include <xzero/util/UnboxedRange.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>
#include <xzero/RegExp.h>

#include <string>
#include <vector>

namespace xzero::flow {

class IRBuilder;
class ConstantArray;

class IRProgram {
 public:
  IRProgram();
  ~IRProgram();

  void dump();

  ConstantBoolean* getBoolean(bool literal) {
    return literal ? &trueLiteral_ : &falseLiteral_;
  }
  ConstantInt* get(int64_t literal) {
    return get<ConstantInt>(numbers_, literal);
  }
  ConstantString* get(const std::string& literal) {
    return get<ConstantString>(strings_, literal);
  }
  ConstantIP* get(const IPAddress& literal) {
    return get<ConstantIP>(ipaddrs_, literal);
  }
  ConstantCidr* get(const Cidr& literal) {
    return get<ConstantCidr>(cidrs_, literal);
  }
  ConstantRegExp* get(const RegExp& literal) {
    return get<ConstantRegExp>(regexps_, literal);
  }
  ConstantArray* get(const std::vector<Constant*>& elems) {
    return get<ConstantArray>(constantArrays_, elems);
  }

  IRBuiltinHandler* findBuiltinHandler(const Signature& sig) const {
    for (size_t i = 0, e = builtinHandlers_.size(); i != e; ++i)
      if (builtinHandlers_[i]->signature() == sig)
        return builtinHandlers_[i].get();

    return nullptr;
  }

  IRBuiltinHandler* getBuiltinHandler(const NativeCallback& cb) {
    for (size_t i = 0, e = builtinHandlers_.size(); i != e; ++i)
      if (builtinHandlers_[i]->signature() == cb.signature())
        return builtinHandlers_[i].get();

    builtinHandlers_.emplace_back(std::make_unique<IRBuiltinHandler>(cb));
    return builtinHandlers_.back().get();
  }

  IRBuiltinFunction* getBuiltinFunction(const NativeCallback& cb) {
    for (size_t i = 0, e = builtinFunctions_.size(); i != e; ++i)
      if (builtinFunctions_[i]->signature() == cb.signature())
        return builtinFunctions_[i].get();

    builtinFunctions_.emplace_back(std::make_unique<IRBuiltinFunction>(cb));
    return builtinFunctions_.back().get();
  }

  template <typename T, typename U> T* get(std::vector<T>& table, const U& literal);
  template <typename T, typename U> T* get(std::vector<std::unique_ptr<T>>& table, const U& literal);

  void addImport(const std::string& name, const std::string& path) {
    modules_.push_back(std::make_pair(name, path));
  }
  void setModules(
      const std::vector<std::pair<std::string, std::string>>& modules) {
    modules_ = modules;
  }

  const std::vector<std::pair<std::string, std::string>>& modules() const {
    return modules_;
  }

  auto handlers() { return unbox(handlers_); }

  IRHandler* findHandler(const std::string& name) {
    for (IRHandler* handler: handlers())
      if (handler->name() == name)
        return handler;

    return nullptr;
  }

  IRHandler* createHandler(const std::string& name);

  /**
   * Performs given transformation on all handlers by given type.
   *
   * @see HandlerPass
   */
  template <typename TheHandlerPass, typename... Args>
  size_t transform(Args&&... args) {
    size_t count = 0;
    for (IRHandler* handler : handlers()) {
      count += handler->transform<TheHandlerPass>(args...);
    }
    return count;
  }

 private:
  std::vector<std::pair<std::string, std::string>> modules_;
  ConstantBoolean trueLiteral_;
  ConstantBoolean falseLiteral_;
  std::vector<ConstantArray> constantArrays_;
  std::vector<std::unique_ptr<ConstantInt>> numbers_;
  std::vector<std::unique_ptr<ConstantString>> strings_;
  std::vector<std::unique_ptr<ConstantIP>> ipaddrs_;
  std::vector<std::unique_ptr<ConstantCidr>> cidrs_;
  std::vector<std::unique_ptr<ConstantRegExp>> regexps_;
  std::vector<std::unique_ptr<IRBuiltinFunction>> builtinFunctions_;
  std::vector<std::unique_ptr<IRBuiltinHandler>> builtinHandlers_;
  std::vector<std::unique_ptr<IRHandler>> handlers_;

  friend class IRBuilder;
};

}  // namespace xzero::flow
