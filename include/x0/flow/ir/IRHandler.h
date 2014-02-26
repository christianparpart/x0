/* <x0/flow/ir/IRHandler.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Constant.h>

#include <string>
#include <vector>
#include <list>

namespace x0 {

class BasicBlock;
class IRProgram;
class IRBuilder;

class X0_API IRHandler : public Constant {
public:
    IRHandler(size_t id, const std::string& name);
    ~IRHandler();

    BasicBlock* createBlock(const std::string& name = "");

    BasicBlock* setEntryPoint(BasicBlock* bb);
    BasicBlock* entryPoint() const { return entryPoint_; }

    IRProgram* parent() const { return parent_; }
    void setParent(IRProgram* prog) { parent_ = prog; }

    void dump() override;

    const std::list<BasicBlock*>& basicBlocks() const { return blocks_; }

private:
    IRProgram* parent_;
    BasicBlock* entryPoint_;
    std::list<BasicBlock*> blocks_;

    friend class IRBuilder;
};

} // namespace x0
