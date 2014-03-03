/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/Instr.h>
#include <x0/flow/ir/Instructions.h>
#include <x0/flow/ir/IRHandler.h>
#include <algorithm>
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

    for (BasicBlock* bb: predecessors()) {
        bb->unlinkSuccessor(this);
    }

    for (BasicBlock* bb: successors()) {
        unlinkSuccessor(bb);
    }
}

BranchInstr* BasicBlock::getTerminator() const
{
    return dynamic_cast<BranchInstr*>(code_.back());
}

Instr* BasicBlock::remove(Instr* instr)
{
    auto i = std::find(code_.begin(), code_.end(), instr);
    assert(i != code_.end());
    code_.erase(i);
    instr->setParent(nullptr);
    return instr;
}

void BasicBlock::push_back(Instr* instr)
{
    assert(instr != nullptr);
    assert(instr->parent() == nullptr);

    instr->setParent(this);
    code_.push_back(instr);

    // XXX the resulting type of a basic block always equals the one of its last inserted instruction
    setType(instr->type());
}

void BasicBlock::merge_back(BasicBlock* bb)
{
    assert(getTerminator() == nullptr);
    assert(!"FIXME: implementation unclear yet");

    for (Instr* instr: bb->code_) {
        instr->setParent(nullptr);
        push_back(instr);
    }
    bb->code_.clear();

    for (BasicBlock* succ: bb->successors()) {
        linkSuccessor(succ);
    }

    for (BasicBlock* succ: successors()) {
        bb->unlinkSuccessor(succ);
    }
}

void BasicBlock::moveAfter(BasicBlock* otherBB)
{
    assert(parent() == otherBB->parent());

    IRHandler* handler = parent();
    auto& list = handler->basicBlocks();

    list.remove(otherBB);

    auto i = std::find(list.begin(), list.end(), this);
    ++i;
    list.insert(i, otherBB);
}

void BasicBlock::moveBefore(BasicBlock* otherBB)
{
    assert(parent() == otherBB->parent());

    IRHandler* handler = parent();
    auto& list = handler->basicBlocks();

    list.remove(otherBB);

    auto i = std::find(list.begin(), list.end(), this);
    list.insert(i, otherBB);
}

bool BasicBlock::isAfter(const BasicBlock* otherBB) const
{
    assert(parent() == otherBB->parent());

    const auto& list = parent()->basicBlocks();
    auto i = std::find(list.cbegin(), list.cend(), this);

    if (i == list.cend())
        return false;

    ++i;

    return *i == otherBB;
}

void BasicBlock::dump()
{
    int n = printf("%%%s:", name().c_str());
    if (!predecessors().empty()) {
        printf("%*c; [preds: ", 20 - n, ' ');
        for (size_t i = 0, e = predecessors().size(); i != e; ++i) {
            if (i) printf(", ");
            printf("%%%s", predecessors()[i]->name().c_str());
        }
        printf("]");
    }
    printf("\n");

    if (!successors().empty()) {
        printf("%20c; [succs: ", ' ');
        for (size_t i = 0, e = successors().size(); i != e; ++i) {
            if (i) printf(", ");
            printf("%%%s", successors()[i]->name().c_str());
        }
        printf("]\n");
    }

    for (size_t i = 0, e = code_.size(); i != e; ++i) {
        code_[i]->dump();
    }

    printf("\n");
}

void BasicBlock::linkSuccessor(BasicBlock* successor)
{
    assert(successor != nullptr);

    successors().push_back(successor);
    successor->predecessors().push_back(this);
}

void BasicBlock::unlinkSuccessor(BasicBlock* successor)
{
    assert(successor != nullptr);

    printf("BasicBlock(%s).unlinkSuccessor(%s)\n", name().c_str(), successor->name().c_str());
    printf("successor's preds:");
    for (const BasicBlock* bb: successor->predecessors()) {
        printf(" %s", bb->name().c_str());
    }
    printf("\n");

    auto p = std::find(successor->predecessors_.begin(), successor->predecessors_.end(), this);
    assert(p != successor->predecessors_.end());
    successor->predecessors_.erase(p);

    auto s = std::find(successors_.begin(), successors_.end(), successor);
    assert(s != successors_.end());
    successors_.erase(s);
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
