#include <x0/flow/transform/EmptyBlockElimination.h>
#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/Instructions.h>
#include <list>

namespace x0 {

size_t EmptyBlockElimination::run(IRHandler* handler)
{
    std::list<BasicBlock*> eliminated;

    for (BasicBlock* bb: handler->basicBlocks()) {
        if (bb->instructions().size() != 1)
            continue;

        if (BrInstr* br = dynamic_cast<BrInstr*>(bb->getTerminator())) {
            BasicBlock* newSuccessor = br->targetBlock();
            for (BasicBlock* pred: bb->predecessors()) {
                pred->getTerminator()->replaceOperand(bb, newSuccessor);
                eliminated.push_back(bb);
            }

            // now, this BB is ideally not referenced by any other BB
        }
    }

    for (BasicBlock* bb: eliminated) {
        bb->parent()->remove(bb);
    }

    return eliminated.size();
}

} // namespace x0
