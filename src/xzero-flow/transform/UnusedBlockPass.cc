// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/transform/UnusedBlockPass.h>
#include <xzero-flow/ir/BasicBlock.h>
#include <xzero-flow/ir/Instructions.h>
#include <xzero-flow/ir/IRHandler.h>
#include <xzero-flow/ir/Instructions.h>
#include <xzero/logging.h>
#include <list>

namespace xzero::flow {

bool UnusedBlockPass::run(IRHandler* handler) {
  std::list<BasicBlock*> unused;

  for (BasicBlock* bb : handler->basicBlocks()) {
    if (bb == handler->getEntryBlock())
      continue;

    if (!bb->predecessors().empty())
      continue;

    unused.push_back(bb);
  }

  for (BasicBlock* bb : unused) {
    logTrace("flow: removing unused BasicBlock $0", bb->name());
    handler->erase(bb);
  }

  return !unused.empty();
}

}  // namespace xzero::flow
