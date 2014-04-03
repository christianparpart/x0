#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/HandlerPass.h>

namespace x0 {

class BasicBlock;

/**
 * Eliminates empty blocks, that are just jumping to the next block.
 */
class X0_API InstructionElimination : public HandlerPass {
public:
    InstructionElimination() : HandlerPass("InstructionElimination") {}

    bool run(IRHandler* handler) override;

private:
    bool rewriteCondBrToSameBranches(BasicBlock* bb);
    bool eliminateLinearBr(BasicBlock* bb);
    bool foldConstantCondBr(BasicBlock* bb);
    bool branchToExit(BasicBlock* bb);
};

} // namespace x0
