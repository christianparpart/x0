// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/LiteralType.h>  // FlowNumber
#include <flow/util/unbox.h>
#include <flow/vm/ConstantPool.h>
#include <flow/vm/Instruction.h>

#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>

#include <vector>
#include <utility>
#include <memory>

namespace xzero::flow {

class NativeCallback;
class Runner;
class Runtime;
class Handler;
class Match;
class MatchDef;
class ConstantPool;

namespace diagnostics {
  class Report;
}

class Program {
 public:
  explicit Program(ConstantPool&& cp);
  Program(Program&) = delete;
  Program& operator=(Program&) = delete;
  ~Program();

  const ConstantPool& constants() const noexcept { return cp_; }
  ConstantPool& constants() noexcept { return cp_; }

  // accessors to linked data
  const Match* match(size_t index) const { return matches_[index].get(); }
  Handler* handler(size_t index) const;
  NativeCallback* nativeHandler(size_t index) const {
    return nativeHandlers_[index];
  }
  NativeCallback* nativeFunction(size_t index) const {
    return nativeFunctions_[index];
  }

  // bulk accessors
  auto matches() { return util::unbox(matches_); }

  std::vector<std::string> handlerNames() const;
  int indexOf(const Handler* handler) const noexcept;
  Handler* findHandler(const std::string& name) const noexcept;

  bool link(Runtime* runtime, diagnostics::Report* report);

  void dump();

 private:
  void setup();

  // builders
  using Code = ConstantPool::Code;
  Handler* createHandler(const std::string& name);
  Handler* createHandler(const std::string& name, const Code& instructions);

 private:
  ConstantPool cp_;

  // linked data
  Runtime* runtime_;
  mutable std::vector<std::unique_ptr<Handler>> handlers_;
  std::vector<std::unique_ptr<Match>> matches_;
  std::vector<NativeCallback*> nativeHandlers_;
  std::vector<NativeCallback*> nativeFunctions_;
};

}  // namespace xzero::flow
