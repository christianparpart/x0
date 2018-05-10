// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/vm/Runtime.h>
#include <flow/ir/IRProgram.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/BasicBlock.h>
#include <flow/ir/Instructions.h>
#include <flow/NativeCallback.h>

namespace xzero::flow {

Runtime::~Runtime() {
}

NativeCallback& Runtime::registerHandler(const std::string& name) {
  builtins_.push_back(std::make_unique<NativeCallback>(this, name));
  return *builtins_[builtins_.size() - 1];
}

NativeCallback& Runtime::registerFunction(const std::string& name,
                                          LiteralType returnType) {
  builtins_.push_back(std::make_unique<NativeCallback>(this, name, returnType));
  return *builtins_[builtins_.size() - 1];
}

NativeCallback* Runtime::find(const std::string& signature) const noexcept {
  for (auto& callback : builtins_) {
    if (callback->signature().to_s() == signature) {
      return callback.get();
    }
  }

  return nullptr;
}

NativeCallback* Runtime::find(const Signature& signature) const noexcept {
  return find(signature.to_s());
}

bool Runtime::verifyNativeCalls(IRProgram* program, IRBuilder* builder) {
  std::list<std::pair<Instr*, NativeCallback*>> calls;

  for (IRHandler* handler : program->handlers()) {
    for (BasicBlock* bb : handler->basicBlocks()) {
      for (Instr* instr : bb->instructions()) {
        if (auto ci = dynamic_cast<CallInstr*>(instr)) {
          if (auto native = find(ci->callee()->signature())) {
            calls.push_back(std::make_pair(instr, native));
          }
        } else if (auto hi = dynamic_cast<HandlerCallInstr*>(instr)) {
          if (auto native = find(hi->callee()->signature())) {
            calls.push_back(std::make_pair(instr, native));
          }
        }
      }
    }
  }

  for (auto call : calls) {
    if (!call.second->verify(call.first, builder)) {
      return false;
    }
  }

  return true;
}

}  // namespace xzero::flow
