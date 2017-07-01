// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/transform/MergeBlockPass.h>
#include <xzero-flow/ir/BasicBlock.h>
#include <xzero-flow/ir/Instructions.h>
#include <xzero-flow/ir/IRHandler.h>
#include <xzero-flow/ir/Instructions.h>
#include <unordered_map>
#include <list>

namespace xzero {
namespace flow {

bool isSameInstructions(BasicBlock* a, BasicBlock* b) {
  if (a->instructions().size() != b->instructions().size())
    return false;

  for (size_t i = 0, e = a->instructions().size(); i != e; ++i)
    if (!IsSameInstruction::test(a->instructions()[i],b->instructions()[i]))
      return false;

  return true;
}

bool isSameSuccessors(BasicBlock* a, BasicBlock* b) {
  if (a->successors().size() != b->successors().size())
    return false;

  for (size_t i = 0, e = a->successors().size(); i != e; ++i)
    if (a->successors()[i] != b->successors()[i])
      return false;

  return true;
}

bool MergeBlockPass::run(IRHandler* handler) {
  std::list<std::list<BasicBlock*>> uniques;

  for (BasicBlock* bb : handler->basicBlocks()) {
    bool found = false;
    // check if we already have a BB that is equal
    for (auto& uniq: uniques) {
      for (BasicBlock* otherBB: uniq) {
        if (isSameInstructions(bb, otherBB) && isSameSuccessors(bb, otherBB)) {
          uniq.emplace_back(bb);
          found = true;
          break;
        }
      }
      if (found) {
        break;
      }
    }
    if (!found) {
      // add new list and add himself to it
      uniques.push_back({bb});
    }
  }

  for (std::list<BasicBlock*>& uniq: uniques) {
    if (uniq.size() > 1) {
      for (auto i = ++uniq.begin(), e = uniq.end(); i != e; ++i) {
        for (BasicBlock* pred: (*i)->predecessors()) {
          pred->getTerminator()->replaceOperand(*i, uniq.front());
        }
      }
    }
  }

  return false;
}

}  // namespace flow
}  // namespace xzero
