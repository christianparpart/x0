// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/BasicBlock.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/Instructions.h>
#include <flow/ir/Instructions.h>
#include <flow/transform/UnusedBlockPass.h>

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
    // FLOW_TRACE("flow: removing unused BasicBlock {}", bb->name());
    handler->erase(bb);
  }

  return !unused.empty();
}

}  // namespace xzero::flow
