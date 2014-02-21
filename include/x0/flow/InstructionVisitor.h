#pragma once

namespace x0 {

class AllocaInstr;
class ArraySetInstr;
class StoreInstr;
class LoadInstr;
class CallInstr;
class VmInstr;
class PhiNode;
class BranchInstr;
class CondBrInstr;
class BrInstr;
class RetInstr;
class MatchInstr;

class X0_API InstructionVisitor {
public:
    virtual ~InstructionVisitor() {};

    virtual void visit(AllocaInstr& instr) = 0;
    virtual void visit(ArraySetInstr& instr) = 0;
    virtual void visit(StoreInstr& instr) = 0;
    virtual void visit(LoadInstr& instr) = 0;
    virtual void visit(CallInstr& instr) = 0;
    virtual void visit(VmInstr& instr) = 0;
    virtual void visit(PhiNode& instr) = 0;
    virtual void visit(BranchInstr& instr) = 0;
    virtual void visit(CondBrInstr& instr) = 0;
    virtual void visit(BrInstr& instr) = 0;
    virtual void visit(RetInstr& instr) = 0;
    virtual void visit(MatchInstr& instr) = 0;
};

} // namespace x0
