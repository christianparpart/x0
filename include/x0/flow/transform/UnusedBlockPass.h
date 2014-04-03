#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/HandlerPass.h>

namespace x0 {

class BasicBlock;

/**
 * Eliminates empty blocks, that are just jumping to the next block.
 */
class X0_API UnusedBlockPass : public HandlerPass {
public:
    UnusedBlockPass() : HandlerPass("UnusedBlockPass") {}

    bool run(IRHandler* handler) override;

private:
    bool rewriteCondBrToSameBranches(BasicBlock* bb);
    bool foldConstantCondBr(BasicBlock* bb);
    bool branchToExit(BasicBlock* bb);
};

} // namespace x0
