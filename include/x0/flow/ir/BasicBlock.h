/* <x0/flow/ir/BasicBlock.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#pragma once

#include <x0/Api.h>
#include <x0/flow/ir/Value.h>
#include <x0/flow/ir/InstructionVisitor.h>
#include <x0/flow/vm/Instruction.h>
#include <x0/flow/vm/MatchClass.h>
#include <x0/flow/vm/Signature.h>
#include <x0/IPAddress.h>
#include <x0/RegExp.h>
#include <x0/Cidr.h>

#include <string>
#include <vector>

namespace x0 {

class Instr;
class IRHandler;
class IRBuilder;

class X0_API BasicBlock : public Value {
public:
    explicit BasicBlock(const std::string& name);
    ~BasicBlock();

    IRHandler* parent() const { return parent_; }
    void setParent(IRHandler* handler) { parent_ = handler; }

    const std::vector<Instr*>& instructions() const { return code_; }
    std::vector<Instr*>& instructions() { return code_; }

    void dump() override;

    void link(BasicBlock* successor);
    void unlink(BasicBlock* successor);

    /** Retrieves all predecessors of given basic block. */
    std::vector<BasicBlock*>& predecessors() { return predecessors_; }

    /** Retrieves all uccessors of the given basic block. */
    std::vector<BasicBlock*>& successors() { return successors_; }

    /** Retrieves all dominators of given basic block. */
    std::vector<BasicBlock*> dominators();

    /** Retrieves all immediate dominators of given basic block. */
    std::vector<BasicBlock*> immediateDominators();

private:
    void collectIDom(std::vector<BasicBlock*>& output);

private:
    IRHandler* parent_;
    std::vector<Instr*> code_;
    std::vector<BasicBlock*> predecessors_;
    std::vector<BasicBlock*> successors_;

    friend class IRBuilder;
    friend class Instr;
};

} // namespace x0
