#include <x0/flow/transform/EmptyBlockElimination.h>
#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/Instructions.h>
#include <list>

namespace x0 {

bool EmptyBlockElimination::run(IRHandler* handler)
{
    std::list<BasicBlock*> eliminated;

    for (BasicBlock* bb: handler->basicBlocks()) {
        if (bb->instructions().size() != 1)
            continue;

        if (bb->predecessors().empty())
            continue;

        if (BrInstr* br = dynamic_cast<BrInstr*>(bb->getTerminator())) {
            //printf("- eliminate single-BR-BB: %s\n", bb->name().c_str());
            BasicBlock* newSuccessor = br->targetBlock();
            for (BasicBlock* pred: bb->predecessors()) {
                //printf("  - replace pred(%s)'s succ to %s\n", pred->name().c_str(), newSuccessor->name().c_str());
                pred->getTerminator()->replaceOperand(bb, newSuccessor);
                eliminated.push_back(bb);
            }

            // now, this BB is ideally not referenced by any other BB
        }
    }

    for (BasicBlock* bb: eliminated) {
        bb->parent()->remove(bb);
        delete bb;
    }

    return eliminated.size() > 0;
}

} // namespace x0
