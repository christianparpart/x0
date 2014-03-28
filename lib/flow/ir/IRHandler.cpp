/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/BasicBlock.h>
#include <algorithm>
#include <assert.h>

namespace x0 {

using namespace FlowVM;

IRHandler::IRHandler(const std::string& name) :
    Constant(FlowType::Handler, name),
    parent_(nullptr),
    blocks_()
{
}

IRHandler::~IRHandler()
{
    while (!blocks_.empty()) {
        auto i = blocks_.begin();
        auto e = blocks_.end();

        while (i != e) {
            BasicBlock* bb = *i;

            if (bb->predecessors().empty()) {
                delete bb;
                auto k = i;
                ++i;
                blocks_.erase(k);
            } else {
                // skip BBs that other BBs still point to (we never point to ourself).
                ++i;
            }
        }
    }
}

void IRHandler::dump()
{
    printf(".handler %s %*c; entryPoint = %%%s\n", name().c_str(), 10 - (int)name().size(), ' ', entryPoint()->name().c_str());

    for (BasicBlock* bb: blocks_)
        bb->dump();

    printf("\n");
}

void IRHandler::remove(BasicBlock* bb)
{
    auto i = std::find(blocks_.begin(), blocks_.end(), bb);
    assert(i != blocks_.end() && "Given basic block must be a member of this handler to be removed.");
    blocks_.erase(i);
}

void IRHandler::verify()
{
    for (BasicBlock* bb: blocks_) {
        bb->verify();
    }
}

} // namespace x0
