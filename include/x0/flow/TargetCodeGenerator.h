#pragma once

#include <x0/flow/FlowType.h>
#include <x0/flow/ir/InstructionVisitor.h>
#include <x0/flow/vm/Match.h>
#include <x0/IPAddress.h>
#include <x0/Cidr.h>
#include <x0/Api.h>

#include <string>
#include <vector>
#include <utility>
#include <memory>

namespace x0 {

class Value;
class Instr;
class IRProgram;
class IRHandler;
class BasicBlock;

namespace FlowVM {
    class Program;
}

class X0_API TargetCodeGenerator : public InstructionVisitor {
public:
    TargetCodeGenerator();
    ~TargetCodeGenerator();

    std::unique_ptr<FlowVM::Program> generate(IRProgram* program);

protected:
    void generate(IRHandler* handler);
    size_t handlerRef(IRHandler* handler);

    size_t emit(FlowVM::Opcode opc) { return emit(FlowVM::makeInstruction(opc)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1) { return emit(FlowVM::makeInstruction(opc, op1)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1, FlowVM::Operand op2) { return emit(FlowVM::makeInstruction(opc, op1, op2)); }
    size_t emit(FlowVM::Opcode opc, FlowVM::Operand op1, FlowVM::Operand op2, FlowVM::Operand op3) { return emit(FlowVM::makeInstruction(opc, op1, op2, op3)); }
    size_t emit(FlowVM::Instruction instr);
    size_t emitBinaryAssoc(Instr& instr, FlowVM::Opcode rr, FlowVM::Opcode ri);
    size_t emitBinaryAssoc(Instr& instr, FlowVM::Opcode rr);
    size_t emitBinary(Instr& instr, FlowVM::Opcode rr);
    size_t emitUnary(Instr& instr, FlowVM::Opcode r);

    FlowVM::Operand getRegister(Value* value);
    FlowVM::Operand getConstantInt(Value* value);
    FlowVM::Operand getLabel(BasicBlock* bb);

    size_t allocate(size_t count, Value* alias) { return allocate(count, *alias); }
    size_t allocate(size_t count, Value& alias);
    size_t allocate(size_t count);
    void free(size_t base, size_t count);

    // storage
    void visit(AllocaInstr& instr) override;
    void visit(ArraySetInstr& instr) override;
    void visit(StoreInstr& instr) override;
    void visit(LoadInstr& instr) override;
    void visit(PhiNode& instr) override;

    // calls
    void visit(CallInstr& instr) override;
    void visit(HandlerCallInstr& instr) override;

    // terminator
    void visit(CondBrInstr& instr) override;
    void visit(BrInstr& instr) override;
    void visit(RetInstr& instr) override;
    void visit(MatchInstr& instr) override;

    // type cast
    void visit(CastInstr& instr) override;

    // numeric
    void visit(INegInstr& instr) override;
    void visit(INotInstr& instr) override;
    void visit(IAddInstr& instr) override;
    void visit(ISubInstr& instr) override;
    void visit(IMulInstr& instr) override;
    void visit(IDivInstr& instr) override;
    void visit(IRemInstr& instr) override;
    void visit(IPowInstr& instr) override;
    void visit(IAndInstr& instr) override;
    void visit(IOrInstr& instr) override;
    void visit(IXorInstr& instr) override;
    void visit(IShlInstr& instr) override;
    void visit(IShrInstr& instr) override;
    void visit(ICmpEQInstr& instr) override;
    void visit(ICmpNEInstr& instr) override;
    void visit(ICmpLEInstr& instr) override;
    void visit(ICmpGEInstr& instr) override;
    void visit(ICmpLTInstr& instr) override;
    void visit(ICmpGTInstr& instr) override;

    // boolean
    void visit(BNotInstr& instr) override;
    void visit(BAndInstr& instr) override;
    void visit(BOrInstr& instr) override;
    void visit(BXorInstr& instr) override;

    // string
    void visit(SLenInstr& instr) override;
    void visit(SIsEmptyInstr& instr) override;
    void visit(SAddInstr& instr) override;
    void visit(SSubStrInstr& instr) override;
    void visit(SCmpEQInstr& instr) override;
    void visit(SCmpNEInstr& instr) override;
    void visit(SCmpLEInstr& instr) override;
    void visit(SCmpGEInstr& instr) override;
    void visit(SCmpLTInstr& instr) override;
    void visit(SCmpGTInstr& instr) override;
    void visit(SCmpREInstr& instr) override;
    void visit(SCmpBegInstr& instr) override;
    void visit(SCmpEndInstr& instr) override;
    void visit(SInInstr& instr) override;

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
