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
class TerminateInstr;
class IRHandler;
class IRBuilder;

/**
 * An SSA based instruction basic block.
 *
 * @see Instr, IRHandler, IRBuilder
 */
class X0_API BasicBlock : public Value {
public:
    explicit BasicBlock(const std::string& name);
    ~BasicBlock();

    IRHandler* parent() const { return parent_; }
    void setParent(IRHandler* handler) { parent_ = handler; }

    /*!
     * Retrieves the last terminating instruction in this basic block.
     *
     * This instruction must be a termination instruction, such as
     * a branching instruction or a handler terminating instruction.
     *
     * @see BrInstr, CondBrInstr, MatchInstr, RetInstr
     */
    TerminateInstr* getTerminator() const;

    /**
     * Retrieves the linear ordered list of instructions of instructions in this basic block.
     */
    const std::vector<Instr*>& instructions() const { return code_; }

    /**
     * Appends a new instruction, \p instr, to this basic block.
     *
     * The basic block will take over ownership of the given instruction.
     */
    void push_back(Instr* instr);

    /**
     * Removes given instruction from this basic block.
     *
     * The basic block will pass ownership of the given instruction to the caller.
     * That means, the caller has to either delete \p childInstr or transfer it to another basic block.
     *
     * @see push_back()
     */
    Instr* remove(Instr* childInstr);

    /**
     * Merges given basic block's instructions into this ones end.
     *
     * The passed basic block's instructions will not be touched.
     */
    void merge_back(BasicBlock* bb);

    /**
     * Moves this basic block after the other basic block, \p otherBB.
     *
     * @param otherBB the future prior basic block.
     *
     * In a function, all basic blocks (starting from the entry block)
     * will be aligned linear into the execution segment.
     *
     * This function moves the given basic block directly after
     * the other basic block, \p otherBB.
     *
     * @see moveBefore()
     */
    void moveAfter(BasicBlock* otherBB);

    /**
     * Moves this basic block before the other basic block, \p otherBB.
     *
     * @see moveAfter()
     */
    void moveBefore(BasicBlock* otherBB);

    /**
     * Tests whether or not given block is straight-line located after this block.
     *
     * @retval true \p otherBB is straight-line located after this block.
     * @retval false \p otherBB is not straight-line located after this block.
     *
     * @see moveAfter()
     */
    bool isAfter(const BasicBlock* otherBB) const;

    /**
     * Links given \p successor basic block to this predecessor.
     *
     * @param successor the basic block to link as an successor of this basic block.
     *
     * This will also automatically link this basic block as
     * future predecessor of the \p successor.
     *
     * @see unlinkSuccessor()
     * @see successors(), predecessors()
     */
    void linkSuccessor(BasicBlock* successor);

    /**
     * Unlink given \p successor basic block from this predecessor.
     *
     * @see linkSuccessor()
     * @see successors(), predecessors()
     */
    void unlinkSuccessor(BasicBlock* successor);

    /** Retrieves all predecessors of given basic block. */
    std::vector<BasicBlock*>& predecessors() { return predecessors_; }

    /** Retrieves all uccessors of the given basic block. */
    std::vector<BasicBlock*>& successors() { return successors_; }
    const std::vector<BasicBlock*>& successors() const { return successors_; }

    /** Retrieves all dominators of given basic block. */
    std::vector<BasicBlock*> dominators();

    /** Retrieves all immediate dominators of given basic block. */
    std::vector<BasicBlock*> immediateDominators();

    void dump() override;

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
