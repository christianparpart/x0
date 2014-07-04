// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Value.h>
#include <x0/flow/ir/ConstantValue.h>
#include <x0/flow/ir/IRBuiltinHandler.h>
#include <x0/flow/ir/IRBuiltinFunction.h>
#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/vm/Signature.h>
#include <x0/IPAddress.h>
#include <x0/RegExp.h>
#include <x0/Cidr.h>

#include <string>
#include <vector>

namespace x0 {

class IRBuilder;
class ConstantArray;

class X0_API IRProgram {
 public:
  IRProgram();
  ~IRProgram();

  void dump();

  ConstantBoolean* getBoolean(bool literal) {
    return literal ? trueLiteral_ : falseLiteral_;
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

  // const std::vector<ConstantArray*>& constantArrays() const { return
  // constantArrays_; }

  IRBuiltinHandler* getBuiltinHandler(const FlowVM::Signature& sig) {
    return get<IRBuiltinHandler>(builtinHandlers_, sig);
  }
  IRBuiltinFunction* getBuiltinFunction(const FlowVM::Signature& sig) {
    return get<IRBuiltinFunction>(builtinFunctions_, sig);
  }

  template <typename T, typename U>
  T* get(std::vector<T*>& table, const U& literal);

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
  const std::vector<IRHandler*>& handlers() const { return handlers_; }

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
  std::vector<ConstantArray*> constantArrays_;
  std::vector<ConstantInt*> numbers_;
  std::vector<ConstantString*> strings_;
  std::vector<ConstantIP*> ipaddrs_;
  std::vector<ConstantCidr*> cidrs_;
  std::vector<ConstantRegExp*> regexps_;
  std::vector<IRBuiltinFunction*> builtinFunctions_;
  std::vector<IRBuiltinHandler*> builtinHandlers_;
  std::vector<IRHandler*> handlers_;
  ConstantBoolean* trueLiteral_;
  ConstantBoolean* falseLiteral_;

  friend class IRBuilder;
};

}  // namespace x0
