// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/vm/Instruction.h>
#include <xzero/defines.h>
#include <xzero/sysconfig.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace xzero::flow::vm {

class Program;
class Runner;

class Handler {
 public:
  Handler();
  Handler(Program* program,
          const std::string& name,
          const std::vector<Instruction>& instructions);
  Handler(const Handler& handler);
  Handler(Handler&& handler);
  ~Handler();

  Program* program() const { return program_; }

  const std::string& name() const { return name_; }
  void setName(const std::string& name) { name_ = name; }

  size_t stackSize() const { return stackSize_; }

  const std::vector<Instruction>& code() const { return code_; }
  void setCode(const std::vector<Instruction>& code);
  void setCode(std::vector<Instruction>&& code);

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  const std::vector<uint64_t>& directThreadedCode() const {
    return directThreadedCode_;
  }
  std::vector<uint64_t>& directThreadedCode() { return directThreadedCode_; }
#endif

  bool run(void* userdata = nullptr, void* userdata2 = nullptr);

  void disassemble();

 private:
  Program* program_;
  std::string name_;
  size_t stackSize_;
  std::vector<Instruction> code_;
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  std::vector<uint64_t> directThreadedCode_;
#endif
};

}  // namespace xzero::flow::vm
