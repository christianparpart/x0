// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/vm/Handler.h>
#include <xzero-flow/vm/Runner.h>
#include <xzero-flow/vm/Instruction.h>
#include <xzero/logging.h>
#include <xzero/sysconfig.h>

namespace xzero::flow::vm {

#define TRACE(msg...) logTrace("flow.vm.Handler", msg)

Handler::Handler() {
}

Handler::Handler(Program* program,
                 const std::string& name,
                 const std::vector<Instruction>& code)
    : program_(program),
      name_(name),
      stackSize_(computeStackSize(code.data(), code.size())),
      code_(code)
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
      ,
      directThreadedCode_()
#endif
{
  TRACE("Handler.ctor: $0 $1", name_, (long long) this);
}

Handler::Handler(const Handler& v)
    : program_(v.program_),
      name_(v.name_),
      stackSize_(v.stackSize_),
      code_(v.code_)
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
      ,
      directThreadedCode_(v.directThreadedCode_)
#endif
{
  TRACE("Handler.ctor(&): $0 $1", name_, (long long) this);
}

Handler::Handler(Handler&& v)
    : program_(std::move(v.program_)),
      name_(std::move(v.name_)),
      stackSize_(std::move(v.stackSize_)),
      code_(std::move(v.code_))
#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
      ,
      directThreadedCode_(std::move(v.directThreadedCode_))
#endif
{
  TRACE("Handler.ctor(&&): $0 $1", name_, (long long) this);
}

Handler::~Handler() {
  TRACE("~Handler: $0 $1", name_, (long long) this);
}

void Handler::setCode(const std::vector<Instruction>& code) {
  code_ = code;
  stackSize_ = computeStackSize(code_.data(), code_.size());
}

void Handler::setCode(std::vector<Instruction>&& code) {
  code_ = std::move(code);
  stackSize_ = computeStackSize(code_.data(), code_.size());

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  directThreadedCode_.clear();
#endif
}

std::unique_ptr<Runner> Handler::createRunner() {
  return std::unique_ptr<Runner>(new Runner(this));
}

bool Handler::run(void* userdata, void* userdata2) {
  auto runner = createRunner();
  runner->setUserData(userdata, userdata2);
  return runner->run();
}

void Handler::disassemble() {
  printf("%s", vm::disassemble(code_.data(), code_.size()).c_str());
}

}  // namespace xzero::flow::vm
