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

        if (BrInstr* br = dynamic_cast<BrInstr*>(bb->getTerminator())) {
            BasicBlock* newSuccessor = br->targetBlock();
            eliminated.push_back(bb);
            if (bb == handler->getEntryBlock()) {
                handler->setEntryBlock(bb);
                break;
            } else {
                for (BasicBlock* pred: bb->predecessors()) {
                    pred->getTerminator()->replaceOperand(bb, newSuccessor);
                }
            }
        }
    }

    for (BasicBlock* bb: eliminated) {
        bb->parent()->erase(bb);
    }

    return eliminated.size() > 0;
}

} // namespace x0
