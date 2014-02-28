/* <x0/flow/ir/xxx.h>
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
#include <list>

namespace x0 {

class Instr;
class BasicBlock;
class IRHandler;
class IRProgram;
class IRBuilder;

class InstructionVisitor;

// NCMPEQ, NADD, SMATCHR, JMP, EXIT, ...
class X0_API Instr : public Value {
public:
    Instr(FlowType ty, const std::vector<Value*>& ops = {}, const std::string& name = "");
    ~Instr();

    BasicBlock* parent() const { return parent_; }
    void setParent(BasicBlock* bb) { parent_ = bb; }

    const std::vector<Value*>& operands() const { return operands_; }
    std::vector<Value*>& operands() { return operands_; }
    Value* operand(size_t index) const { return operands_[index]; }

    /**
     * Replaces \p old operand with \p replacement.
     *
     * @param old value to replace
     * @param replacement new value to put at the offset of \p old.
     *
     * @returns number of actual performed replacements.
     */
    size_t replaceOperand(Value* old, Value* replacement);

    virtual void accept(InstructionVisitor& v) = 0;

protected:
    void dumpOne(const char* mnemonic);

private:
    BasicBlock* parent_;
    std::vector<Value*> operands_;
};

} // namespace x0
