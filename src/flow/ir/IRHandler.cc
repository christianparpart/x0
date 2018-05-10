// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/IRHandler.h>
#include <flow/ir/BasicBlock.h>
#include <flow/ir/Instructions.h>
#include <flow/util/assert.h>
#include <algorithm>
#include <assert.h>

namespace xzero::flow {

IRHandler::IRHandler(const std::string& name, IRProgram* program)
    : Constant(LiteralType::Handler, name), program_(program), blocks_() {
}

IRHandler::~IRHandler() {
  for (BasicBlock* bb : basicBlocks()) {
    for (Instr* instr : bb->instructions()) {
      instr->clearOperands();
    }
  }

  while (!blocks_.empty()) {
    auto i = blocks_.begin();
    auto e = blocks_.end();

    while (i != e) {
      BasicBlock* bb = i->get();

      if (bb->predecessors().empty()) {
        i = blocks_.erase(i);
      } else {
        // skip BBs that other BBs still point to (we never point to ourself).
        ++i;
      }
    }
  }
}

BasicBlock* IRHandler::createBlock(const std::string& name) {
  blocks_.emplace_back(std::make_unique<BasicBlock>(name, this));
  return blocks_.back().get();
}

void IRHandler::setEntryBlock(BasicBlock* bb) {
  FLOW_ASSERT(bb->getHandler(), "BasicBlock must belong to this handler.");

  auto i = std::find_if(blocks_.begin(), blocks_.end(),
                        [&](const auto& obj) { return obj.get() == bb; });
  FLOW_ASSERT(i != blocks_.end(), "BasicBlock must belong to this handler.");
  std::unique_ptr<BasicBlock> t = std::move(*i);
  blocks_.erase(i);
  blocks_.push_front(std::move(t));
}

void IRHandler::dump() {
  printf(".handler %s %*c; entryPoint = %%%s\n", name().c_str(),
         10 - (int)name().size(), ' ', getEntryBlock()->name().c_str());

  for (auto& bb : blocks_)
    bb->dump();

  printf("\n");
}

bool IRHandler::isAfter(const BasicBlock* bb, const BasicBlock* afterThat) const {
  assert(bb->getHandler() == this);
  assert(afterThat->getHandler() == this);

  auto i = std::find_if(blocks_.cbegin(), blocks_.cend(),
                       [&](const auto& obj) { return obj.get() == bb; });

  if (i == blocks_.cend())
    return false;

  ++i;

  if (i == blocks_.cend())
    return false;

  return i->get() == afterThat;
}

void IRHandler::moveAfter(const BasicBlock* moveable, const BasicBlock* after) {
  assert(moveable->getHandler() == this && after->getHandler() == this);

  auto i = std::find_if(blocks_.begin(), blocks_.end(),
                       [&](const auto& obj) { return obj.get() == moveable; });
  std::unique_ptr<BasicBlock> m = std::move(*i);
  blocks_.erase(i);

  i = std::find_if(blocks_.begin(), blocks_.end(),
                   [&](const auto& obj) { return obj.get() == after; });
  ++i;
  blocks_.insert(i, std::move(m));
}

void IRHandler::moveBefore(const BasicBlock* moveable, const BasicBlock* before) {
  assert(moveable->getHandler() == this && before->getHandler() == this);

  auto i = std::find_if(blocks_.begin(), blocks_.end(),
                        [&](const auto& obj) { return obj.get() == moveable; });
  std::unique_ptr<BasicBlock> m = std::move(*i);
  blocks_.erase(i);

  i = std::find_if(blocks_.begin(), blocks_.end(),
                   [&](const auto& obj) { return obj.get() == before; });
  ++i;
  blocks_.insert(i, std::move(m));
}

void IRHandler::erase(BasicBlock* bb) {
  auto i = std::find_if(blocks_.begin(), blocks_.end(),
                        [&](const auto& obj) { return obj.get() == bb; });
  FLOW_ASSERT(i != blocks_.end(),
              "Given basic block must be a member of this handler to be removed.");

  for (Instr* instr : bb->instructions()) {
    instr->clearOperands();
  }

  if (TerminateInstr* terminator = bb->getTerminator()) {
    (*i)->remove(terminator);
  }

  blocks_.erase(i);
}

void IRHandler::verify() {
  for (std::unique_ptr<BasicBlock>& bb : blocks_) {
    bb->verify();
  }
}

}  // namespace xzero::flow
