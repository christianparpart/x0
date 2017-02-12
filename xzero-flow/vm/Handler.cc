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

namespace xzero {
namespace flow {
namespace vm {

#define TRACE(msg...) logTrace("flow.vm.Handler", msg)

Handler::Handler() {
}

Handler::Handler(std::shared_ptr<Program> program,
                 const std::string& name,
                 const std::vector<Instruction>& code)
    : program_(program),
      name_(name),
      registerCount_(computeRegisterCount(code.data(), code.size())),
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
      registerCount_(v.registerCount_),
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
      registerCount_(std::move(v.registerCount_)),
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
  registerCount_ = computeRegisterCount(code_.data(), code_.size());
}

void Handler::setCode(std::vector<Instruction>&& code) {
  code_ = std::move(code);
  registerCount_ = computeRegisterCount(code_.data(), code_.size());

#if defined(ENABLE_FLOW_DIRECT_THREADED_VM)
  directThreadedCode_.clear();
#endif
}

std::unique_ptr<Runner> Handler::createRunner() {
  TRACE("Handler.createRunner: use_count=$0", shared_from_this().use_count());
  return Runner::create(shared_from_this());
}

bool Handler::run(void* userdata, void* userdata2) {
  auto runner = createRunner();
  runner->setUserData(userdata, userdata2);
  return runner->run();
}

void Handler::disassemble() {
  printf("%s", vm::disassemble(code_.data(), code_.size()).c_str());
}

}  // namespace vm
}  // namespace flow
}  // namespace xzero
