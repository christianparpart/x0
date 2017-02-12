// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero-flow/FlowType.h>
#include <xzero-flow/vm/Params.h>
#include <xzero-flow/vm/Signature.h>
#include <string>
#include <vector>
#include <functional>

namespace xzero {
namespace flow {

class IRProgram;
class IRBuilder;

namespace vm {

typedef uint64_t Value;

class Runner;
class NativeCallback;

class XZERO_FLOW_API Runtime {
 public:
  virtual ~Runtime();

  virtual bool import(const std::string& name, const std::string& path,
                      std::vector<vm::NativeCallback*>* builtins) = 0;

  bool contains(const std::string& signature) const;
  NativeCallback* find(const std::string& signature);
  NativeCallback* find(const Signature& signature);
  const std::vector<NativeCallback*>& builtins() const { return builtins_; }

  NativeCallback& registerHandler(const std::string& name);
  NativeCallback& registerFunction(const std::string& name, FlowType returnType);
  void unregisterNative(const std::string& name);

  void invoke(int id, int argc, Value* argv, Runner* cx);

  /**
   * Verifies all call instructions.
   */
  bool verify(IRProgram* program, IRBuilder* builder);

 private:
  std::vector<NativeCallback*> builtins_;
};

}  // vm
}  // flow
}  // xzero
