#pragma once

#include <x0/Cidr.h>
#include <x0/IPAddress.h>
#include <x0/flow/FlowType.h>
#include <x0/flow/InstructionVisitor.h>
#include <x0/flow/vm/Match.h>
#include <x0/Api.h>

#include <string>
#include <vector>
#include <utility>
#include <memory>

namespace x0 {

class Value;
class IRProgram;
class IRHandler;
class BasicBlock;

namespace FlowVM {
    class Program;
}

class X0_API VMCodeGenerator : public InstructionVisitor {
public:
    VMCodeGenerator();
    ~VMCodeGenerator();

    std::unique_ptr<FlowVM::Program> generate(IRProgram* program);

protected:
    void generate(IRHandler* handler);
    size_t handlerRef(IRHandler* handler);

    size_t emit(FlowVM::Opcode opc) { return emit(FlowVM::makeInstruction(opc)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1) { return emit(FlowVM::makeInstruction(opc, op1)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1, FlowVM::Operand op2) { return emit(FlowVM::makeInstruction(opc, op1, op2)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1, FlowVM::Operand op2, FlowVM::Operand op3) { return emit(FlowVM::makeInstruction(opc, op1, op2, op3)); }
    size_t emit(FlowVM::Instruction instr);

    FlowVM::Operand getRegister(Value* value);
    FlowVM::Operand getConstantInt(Value* value);
    FlowVM::Operand getLabel(BasicBlock* bb);

    size_t allocate(size_t count);
    void free(size_t base, size_t count);

    virtual void visit(AllocaInstr& instr);
    virtual void visit(ArraySetInstr& instr);
    virtual void visit(StoreInstr& instr);
    virtual void visit(LoadInstr& instr);
    virtual void visit(CallInstr& instr);
    virtual void visit(VmInstr& instr);
    virtual void visit(PhiNode& instr);
    virtual void visit(BranchInstr& instr);
    virtual void visit(CondBrInstr& instr);
    virtual void visit(BrInstr& instr);
    virtual void visit(RetInstr& instr);
    virtual void visit(MatchInstr& instr);

private:
    std::vector<std::string> errors_;           //!< list of raised errors during code generation.

    // target program output
    std::vector<FlowNumber> constNumbers_;
    std::vector<std::string> constStrings_;
    std::vector<IPAddress> ipaddrs_;
    std::vector<Cidr> cidrs_;
    std::vector<std::string> regularExpressions_;
    std::vector<FlowVM::MatchDef> matches_;
    std::vector<std::pair<std::string, std::string>> modules_;
    std::vector<std::string> nativeHandlerSignatures_;
    std::vector<std::string> nativeFunctionSignatures_;
    std::vector<std::pair<std::string, std::vector<FlowVM::Instruction>>> handlers_;

    size_t handlerId_;                          //!< current handler's ID
    std::vector<FlowVM::Instruction> code_;     //!< current handler's code 
    std::unordered_map<Value*, Register> variables_;
    std::vector<bool> allocations_;
};


} // namespace x0
