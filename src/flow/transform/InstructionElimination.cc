// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/BasicBlock.h>
#include <flow/ir/IRBuiltinFunction.h>
#include <flow/ir/IRBuiltinHandler.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/Instructions.h>
#include <flow/ir/Instructions.h>
#include <flow/transform/InstructionElimination.h>

#include <list>
#include <unordered_map>

namespace xzero::flow {

bool InstructionElimination::run(IRHandler* handler) {
  for (BasicBlock* bb : handler->basicBlocks()) {
    if (rewriteCondBrToSameBranches(bb)) return true;
    if (eliminateUnusedInstr(bb)) return true;
    if (eliminateLinearBr(bb)) return true;
    if (foldConstantCondBr(bb)) return true;
    if (branchToExit(bb)) return true;
  }

  return false;
}

/*
 * Rewrites CONDBR (%foo, %foo) to BR (%foo) as both target branch pointers
 * point to the same branch.
 */
bool InstructionElimination::rewriteCondBrToSameBranches(BasicBlock* bb) {
  // attempt to eliminate useless condbr
  if (CondBrInstr* condbr = dynamic_cast<CondBrInstr*>(bb->getTerminator())) {
    if (condbr->trueBlock() != condbr->falseBlock())
      return false;

    BasicBlock* nextBB = condbr->trueBlock();

    // remove old terminator
    bb->remove(condbr);

    // create new terminator
    bb->push_back(std::make_unique<BrInstr>(nextBB));

    //FLOW_TRACE("flow: rewrote condbr with true-block == false-block");
    return true;
  }

  return false;
}

bool InstructionElimination::eliminateUnusedInstr(BasicBlock* bb) {
  for (Instr* instr : bb->instructions()) {
    if (auto f = dynamic_cast<CallInstr*>(instr)) {
      if (f->callee()->getNative().isReadOnly()) {
        if (instr->type() != LiteralType::Void && !instr->isUsed()) {
          bb->remove(instr);
          return true;
        }
      }
    }
  }
  return false;
}

/*
 * Eliminates BR instructions to basic blocks that are only referenced by one
 * basic block
 * by eliminating the BR and merging the BR instructions target block at the end
 * of the current block.
 */
bool InstructionElimination::eliminateLinearBr(BasicBlock* bb) {
  // attempt to eliminate useless linear br
  if (BrInstr* br = dynamic_cast<BrInstr*>(bb->getTerminator())) {
    if (br->targetBlock()->predecessors().size() != 1)
      return false;

    if (br->targetBlock()->predecessors().front() != bb)
      return false;

    // we are the only predecessor of BR's target block, so merge them
    BasicBlock* nextBB = br->targetBlock();

    // FLOW_TRACE("flow: eliminate linear BR-instruction from {} to {}", bb->name(), nextBB->name());

    // remove old terminator
    bb->remove(br);

    // merge nextBB
    bb->merge_back(nextBB);

    // destroy unused BB
    //bb->getHandler()->erase(nextBB);

    return true;
  }

  return false;
}

bool InstructionElimination::foldConstantCondBr(BasicBlock* bb) {
  if (auto condbr = dynamic_cast<CondBrInstr*>(bb->getTerminator())) {
    if (auto cond = dynamic_cast<ConstantBoolean*>(condbr->condition())) {
      // FLOW_TRACE("flow: rewrite condbr %{} with constant expression %{}", condbr->name(), cond->name());
      std::pair<BasicBlock*, BasicBlock*> use;

      if (cond->get()) {
        // FLOW_TRACE("if-condition is always true");
        use = std::make_pair(condbr->trueBlock(), condbr->falseBlock());
      } else {
        // FLOW_TRACE("if-condition is always false");
        use = std::make_pair(condbr->falseBlock(), condbr->trueBlock());
      }

      auto x = bb->remove(condbr);
      x.reset(nullptr);
      bb->push_back(std::make_unique<BrInstr>(use.first));
      return true;
    }
  }
  return false;
}

/*
 * Eliminates a superfluous BR instruction to a basic block that just exits.
 *
 * This will highly increase the number of exit points but reduce
 * the number of executed instructions for each path.
 */
bool InstructionElimination::branchToExit(BasicBlock* bb) {
  if (BrInstr* br = dynamic_cast<BrInstr*>(bb->getTerminator())) {
    BasicBlock* targetBB = br->targetBlock();

    if (targetBB->size() != 1)
      return false;

    if (bb->isAfter(targetBB))
      return false;

    if (RetInstr* ret = dynamic_cast<RetInstr*>(targetBB->getTerminator())) {
      bb->remove(br);
      bb->push_back(ret->clone());

      // FLOW_TRACE("flow: eliminate branch-to-exit block");
      return true;
    }
  }

  return false;
}

}  // namespace xzero::flow
