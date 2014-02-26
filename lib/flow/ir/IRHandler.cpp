/*
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/BasicBlock.h>
#include <assert.h>

namespace x0 {

using namespace FlowVM;

IRHandler::IRHandler(size_t id, const std::string& name) :
    Constant(FlowType::Handler, id, name),
    parent_(nullptr),
    entryPoint_(nullptr),
    blocks_()
{
}

IRHandler::~IRHandler()
{
}

BasicBlock* IRHandler::setEntryPoint(BasicBlock* bb)
{
    assert(bb->parent() == this);
    assert(entryPoint_ == nullptr && "QA: changing EP not allowed.");

    entryPoint_ = bb;

    return bb;
}

void IRHandler::dump()
{
    printf(".handler %s ; entryPoint = %%%s\n", name().c_str(), entryPoint_->name().c_str());

    for (BasicBlock* bb: blocks_)
        bb->dump();

    printf("\n");
}

} // namespace x0
