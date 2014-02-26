/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/Instr.h>
#include <assert.h>
#include <math.h>

/*
 * TODO assert() on last instruction in current BB is not a terminator instr.
 */

namespace x0 {

BasicBlock::BasicBlock(const std::string& name) :
    Value(FlowType::Void, name),
    parent_(nullptr),
    code_(),
    predecessors_(),
    successors_()
{
}

BasicBlock::~BasicBlock()
{
    for (auto instr: code_) {
        delete instr;
    }
}

void BasicBlock::dump()
{
    printf("%%%s:\n", name().c_str());
    for (size_t i = 0, e = code_.size(); i != e; ++i) {
        code_[i]->dump();
    }
    printf("\n");
}

void BasicBlock::link(BasicBlock* successor)
{
    assert(successor != nullptr);

    successors().push_back(successor);
    successor->predecessors().push_back(this);
}

void BasicBlock::unlink(BasicBlock* successor)
{
    assert(!"TODO");
}

std::vector<BasicBlock*> BasicBlock::dominators()
{
    std::vector<BasicBlock*> result;
    collectIDom(result);
    result.push_back(this);
    return result;
}

std::vector<BasicBlock*> BasicBlock::immediateDominators()
{
    std::vector<BasicBlock*> result;
    collectIDom(result);
    return result;
}

void BasicBlock::collectIDom(std::vector<BasicBlock*>& output)
{
    for (BasicBlock* p: predecessors()) {
        p->collectIDom(output);
    }
}

} // namespace x0
