// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/ir/PassManager.h>
#include <xzero-flow/ir/HandlerPass.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero/logging.h>

namespace xzero {
namespace flow {

PassManager::PassManager() {
}

PassManager::~PassManager() {
}

void PassManager::registerPass(std::unique_ptr<HandlerPass>&& handlerPass) {
  handlerPasses_.push_back(std::move(handlerPass));
}

void PassManager::run(IRProgram* program) {
  for (IRHandler* handler : program->handlers()) {
    run(handler);
  }
}

void PassManager::run(IRHandler* handler) {
  logTrace("flow: Running optimizations on handler: $0", handler->name());
  for (;;) {
    int changes = 0;
    for (auto& pass : handlerPasses_) {
      logTrace("flow: Running optimization pass: $0", pass->name());
      while (pass->run(handler)) {
        changes++;
      }
    }
    if (!changes) {
      break;
    }
  }
}

}  // namespace flow
}  // namespace xzero
