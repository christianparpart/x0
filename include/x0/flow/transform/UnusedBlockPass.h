// This file is part of the "x0" project, http://xzero.io/
//   (c) 2009-2014 Christian Parpart <trapni@gmail.com>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

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
