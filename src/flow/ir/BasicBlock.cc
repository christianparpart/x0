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
#include <flow/ir/Instr.h>
#include <flow/ir/Instructions.h>
#include <flow/util/assert.h>

#include <algorithm>
#include <iterator>
#include <assert.h>
#include <math.h>

/*
 * TODO assert() on last instruction in current BB is not a terminator instr.
 */

namespace xzero::flow {

BasicBlock::BasicBlock(const std::string& name, IRHandler* parent)
    : Value(LiteralType::Void, name),
      handler_(parent),
      code_(),
      predecessors_(),
      successors_() {
}

BasicBlock::~BasicBlock() {
  // XXX we *must* destruct the code in reverse order to ensure proper release
  // The validity checks will fail otherwise.
  while (!empty()) {
    remove(back());
  }

  FLOW_ASSERT(predecessors_.empty(),
              "Cannot remove a BasicBlock that another BasicBlock still refers to.");

  for (BasicBlock* bb : predecessors_) {
    bb->unlinkSuccessor(this);
  }

  for (BasicBlock* bb : successors_) {
    unlinkSuccessor(bb);
  }
}

TerminateInstr* BasicBlock::getTerminator() const {
  return code_.empty()
      ? nullptr
      : dynamic_cast<TerminateInstr*>(code_.back().get());
}

std::unique_ptr<Instr> BasicBlock::remove(Instr* instr) {
  // if we're removing the terminator instruction
  if (instr == getTerminator()) {
    // then unlink all successors also
    for (Value* operand : instr->operands()) {
      if (BasicBlock* bb = dynamic_cast<BasicBlock*>(operand)) {
        unlinkSuccessor(bb);
      }
    }
  }

  auto i = std::find_if(
      code_.begin(), code_.end(),
      [&](const auto& obj) { return obj.get() == instr; });
  assert(i != code_.end());

  std::unique_ptr<Instr> removedInstr = std::move(*i);
  code_.erase(i);
  instr->setParent(nullptr);
  return removedInstr;
}

std::unique_ptr<Instr> BasicBlock::replace(Instr* oldInstr,
                                           std::unique_ptr<Instr> newInstr) {
  assert(oldInstr->getBasicBlock() == this);
  assert(newInstr->getBasicBlock() == nullptr);

  oldInstr->replaceAllUsesWith(newInstr.get());

  if (oldInstr == getTerminator()) {
    std::unique_ptr<Instr> removedInstr = remove(oldInstr);
    push_back(std::move(newInstr));
    return removedInstr;
  } else {
    assert(dynamic_cast<TerminateInstr*>(newInstr.get()) == nullptr && "Most not be a terminator instruction.");

    auto i = std::find_if(
        code_.begin(), code_.end(),
        [&](const auto& obj) { return obj.get() == oldInstr; });

    assert(i != code_.end());

    oldInstr->setParent(nullptr);
    newInstr->setParent(this);

    std::unique_ptr<Instr> removedInstr = std::move(*i);
    *i = std::move(newInstr);
    return removedInstr;
  }
}

Instr* BasicBlock::push_back(std::unique_ptr<Instr> instr) {
  assert(instr != nullptr);
  assert(instr->getBasicBlock() == nullptr);

  // FIXME: do we need this? I'd say NOPE.
  setType(instr->type());

  instr->setParent(this);

  // are we're now adding the terminate instruction?
  if (dynamic_cast<TerminateInstr*>(instr.get())) {
    // then check for possible successors
    for (auto operand : instr->operands()) {
      if (BasicBlock* bb = dynamic_cast<BasicBlock*>(operand)) {
        linkSuccessor(bb);
      }
    }
  }

  code_.push_back(std::move(instr));
  return code_.back().get();
}

void BasicBlock::merge_back(BasicBlock* bb) {
  assert(getTerminator() == nullptr);

#if 0
  for (const std::unique_ptr<Instr>& instr : bb->code_) {
    push_back(instr->clone());
  }
#else
  for (std::unique_ptr<Instr>& instr : bb->code_) {
    instr->setParent(this);
    if (dynamic_cast<TerminateInstr*>(instr.get())) {
      // then check for possible successors
      for (auto operand : instr->operands()) {
        if (BasicBlock* succ = dynamic_cast<BasicBlock*>(operand)) {
          bb->unlinkSuccessor(succ);
          linkSuccessor(succ);
        }
      }
    }
    code_.push_back(std::move(instr));
  }
  bb->code_.clear();
  for (BasicBlock* succ : bb->successors_) {
    unlinkSuccessor(succ);
  }
  bb->getHandler()->erase(bb);
#endif
}

void BasicBlock::moveAfter(const BasicBlock* otherBB) {
  handler_->moveAfter(this, otherBB);
}

void BasicBlock::moveBefore(const BasicBlock* otherBB) {
  handler_->moveBefore(this, otherBB);
}

bool BasicBlock::isAfter(const BasicBlock* otherBB) const {
  return handler_->isAfter(this, otherBB);
}

void BasicBlock::dump() {
  int n = printf("%%%s:", name().c_str());
  if (!predecessors_.empty()) {
    printf("%*c; [preds: ", 20 - n, ' ');
    for (size_t i = 0, e = predecessors_.size(); i != e; ++i) {
      if (i) printf(", ");
      printf("%%%s", predecessors_[i]->name().c_str());
    }
    printf("]");
  }
  printf("\n");

  if (!successors_.empty()) {
    printf("%20c; [succs: ", ' ');
    for (size_t i = 0, e = successors_.size(); i != e; ++i) {
      if (i) printf(", ");
      printf("%%%s", successors_[i]->name().c_str());
    }
    printf("]\n");
  }

  for (size_t i = 0, e = code_.size(); i != e; ++i) {
    code_[i]->dump();
  }

  printf("\n");
}

void BasicBlock::linkSuccessor(BasicBlock* successor) {
  assert(successor != nullptr);

  successors_.push_back(successor);
  successor->predecessors_.push_back(this);
}

void BasicBlock::unlinkSuccessor(BasicBlock* successor) {
  assert(successor != nullptr);

  auto p = std::find(successor->predecessors_.begin(),
                     successor->predecessors_.end(), this);
  assert(p != successor->predecessors_.end());
  successor->predecessors_.erase(p);

  auto s = std::find(successors_.begin(), successors_.end(), successor);
  assert(s != successors_.end());
  successors_.erase(s);
}

std::vector<BasicBlock*> BasicBlock::dominators() {
  std::vector<BasicBlock*> result;
  collectIDom(result);
  result.push_back(this);
  return result;
}

std::vector<BasicBlock*> BasicBlock::immediateDominators() {
  std::vector<BasicBlock*> result;
  collectIDom(result);
  return result;
}

void BasicBlock::collectIDom(std::vector<BasicBlock*>& output) {
  for (BasicBlock* p : predecessors_) {
    p->collectIDom(output);
  }
}

bool BasicBlock::isComplete() const {
  if (empty())
    return false;

  if (getTerminator())
    return true;

  if (auto instr = dynamic_cast<HandlerCallInstr*>(back()))
    return instr->callee()->getNative().isNeverReturning();

  if (auto instr = dynamic_cast<CallInstr*>(back()))
    return instr->callee()->getNative().isNeverReturning();

  return false;
}

void BasicBlock::verify() {
  FLOW_ASSERT(code_.size() > 0, fmt::format("BasicBlock {}: verify: Must contain at least one instruction.", name()));
  FLOW_ASSERT(isComplete(), fmt::format("BasicBlock {}: verify: Last instruction must be a terminator instruction.", name()));
  FLOW_ASSERT(
      std::find_if(code_.begin(), std::prev(code_.end()), [&](std::unique_ptr<Instr>& instr) -> bool {
        return dynamic_cast<TerminateInstr*>(instr.get()) != nullptr;
      }) == std::prev(code_.end()),
      fmt::format("BasicBlock {}: verify: Found a terminate instruction in the middle of the block.", name()));
}

}  // namespace xzero::flow
