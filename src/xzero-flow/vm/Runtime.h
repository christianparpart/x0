// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero/util/UnboxedRange.h>
#include <xzero-flow/Signature.h>
#include <xzero-flow/LiteralType.h>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace xzero::flow {

class IRProgram;
class IRBuilder;
class NativeCallback;
class Runner;

class Runtime {
 public:
  using Value = uint64_t;

  virtual ~Runtime();

  virtual bool import(const std::string& name, const std::string& path,
                      std::vector<NativeCallback*>* builtins) = 0;

  NativeCallback* find(const std::string& signature) const noexcept;
  NativeCallback* find(const Signature& signature) const noexcept;
  auto builtins() { return unbox(builtins_); }

  NativeCallback& registerHandler(const std::string& name);
  NativeCallback& registerFunction(const std::string& name, LiteralType returnType);

  void invoke(int id, int argc, Value* argv, Runner* cx);

  /**
   * Verifies all call instructions.
   */
  bool verifyNativeCalls(IRProgram* program, IRBuilder* builder);

 private:
  std::vector<std::unique_ptr<NativeCallback>> builtins_;
};

}  // xzero::flow
