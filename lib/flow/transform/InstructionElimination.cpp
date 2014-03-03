#include <x0/flow/transform/InstructionElimination.h>
#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/Instructions.h>
#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/Instructions.h>
#include <list>

namespace x0 {

bool InstructionElimination::run(IRHandler* handler)
{
    for (BasicBlock* bb: handler->basicBlocks()) {
        if (rewriteCondBrToSameBranches(bb))
            return true;

        if (eliminateLinearBr(bb))
            return true;
    }

    return false;
}

bool InstructionElimination::rewriteCondBrToSameBranches(BasicBlock* bb)
{
    // attempt to eliminate useless condbr
    if (CondBrInstr* condbr = dynamic_cast<CondBrInstr*>(bb->getTerminator())) {
        if (condbr->trueBlock() != condbr->falseBlock())
            return false;

        BasicBlock* nextBB = condbr->trueBlock();

        // remove old terminator
        bb->unlinkSuccessor(condbr->trueBlock());
        bb->unlinkSuccessor(condbr->falseBlock());
        delete bb->remove(condbr);

        // create new terminator
        bb->linkSuccessor(nextBB);
        bb->push_back(new BrInstr(nextBB));

        return true;
    }

    return false;
}

bool InstructionElimination::eliminateLinearBr(BasicBlock* bb)
{
    // attempt to eliminate useless linear br
    if (BrInstr* br = dynamic_cast<BrInstr*>(bb->getTerminator())) {
        if (br->targetBlock()->predecessors().size() != 1)
            return false;

        // we are the only predecessor of BR's target block, so merge them

        BasicBlock* nextBB = br->targetBlock();

        // remove old terminator
        bb->unlinkSuccessor(nextBB);
        delete bb->remove(br);

        // merge nextBB
        bb->merge_back(nextBB);

        return true;
    }

    return false;
}

} // namespace x0
