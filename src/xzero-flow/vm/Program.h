// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero-flow/vm/ConstantPool.h>
#include <xzero-flow/vm/Instruction.h>
#include <xzero-flow/FlowType.h>  // FlowNumber
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>
#include <xzero/RegExp.h>

#include <vector>
#include <utility>
#include <memory>

namespace xzero {
namespace flow {
namespace vm {

class Runner;
class Runtime;
class Handler;
class Match;
class MatchDef;
class NativeCallback;
class ConstantPool;

class Program : public std::enable_shared_from_this<Program> {
 public:
  explicit Program(ConstantPool&& cp);
  Program(Program&) = delete;
  Program& operator=(Program&) = delete;
  ~Program();

  const ConstantPool& constants() const { return cp_; }

  // accessors to linked data
  const Match* match(size_t index) const { return matches_[index]; }
  std::shared_ptr<Handler> handler(size_t index) const;
  NativeCallback* nativeHandler(size_t index) const {
    return nativeHandlers_[index];
  }
  NativeCallback* nativeFunction(size_t index) const {
    return nativeFunctions_[index];
  }

  // bulk accessors
  const std::vector<Match*>& matches() const { return matches_; }

  std::vector<std::string> handlerNames() const;
  int indexOf(const Handler* handler) const;
  int indexOf(const std::shared_ptr<Handler>& handler) const;
  std::shared_ptr<Handler> findHandler(const std::string& name) const;

  /**
   * Convenience method to run a handler.
   *
   * @param handlerName The handler's name that is going to be run.
   * @param u1 Opaque userdata value 1.
   * @param u2 Opaque userdata value 2.
   */
  bool run(const std::string& handlerName,
      void* u1 = nullptr, void* u2 = nullptr);

  bool link(Runtime* runtime);

  void dump();

  void setup();

 private:
  // builders
  std::shared_ptr<Handler> createHandler(const std::string& name);
  std::shared_ptr<Handler> createHandler(const std::string& name,
      const std::vector<Instruction>& instructions);

 private:
  ConstantPool cp_;

  // linked data
  Runtime* runtime_;
  mutable std::vector<std::shared_ptr<Handler>> handlers_;
  std::vector<Match*> matches_;
  std::vector<NativeCallback*> nativeHandlers_;
  std::vector<NativeCallback*> nativeFunctions_;
};

}  // namespace vm
}  // namespace flow
}  // namespace xzero
