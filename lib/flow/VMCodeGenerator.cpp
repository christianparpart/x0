#include <x0/flow/VMCodeGenerator.h>
#include <x0/flow/vm/Program.h>
#include <x0/flow/IR.h>

namespace x0 {

using namespace FlowVM;

VMCodeGenerator::VMCodeGenerator() :
    errors_(),
    constNumbers_(),
    constStrings_(),
    ipaddrs_(),
    cidrs_(),
    regularExpressions_(),
    matches_(),
    modules_(),
    nativeHandlerSignatures_(),
    nativeFunctionSignatures_(),
    handlers_(),
    handlerId_(0),
    code_(),
    variables_(),
    allocations_()
{
    // preserve r0, so it'll never be used.
    allocations_.push_back(true);
}

VMCodeGenerator::~VMCodeGenerator()
{
}

std::unique_ptr<FlowVM::Program> VMCodeGenerator::generate(IRProgram* program)
{
    for (IRHandler* handler: program->handlers()) {
        generate(handler);
    }

    return std::unique_ptr<FlowVM::Program>(new FlowVM::Program(
        constNumbers_,
        constStrings_,
        ipaddrs_,
        cidrs_,
        regularExpressions_,
        matches_,
        program->imports(),
        nativeHandlerSignatures_,
        nativeFunctionSignatures_,
        handlers_
    ));
}

void VMCodeGenerator::generate(IRHandler* handler)
{
    // explicitely forward-declare handler, so we can use its ID internally.
    handlerId_ = handlerRef(handler);

    for (BasicBlock* bb: handler->basicBlocks()) {
        for (Instr* instr: bb->instructions()) {
            instr->accept(*this);
        }
    }

    handlers_[handlerId_].second = std::move(code_);
}

/**
 * Retrieves the program's handler ID for given handler, possibly forward-declaring given handler if not (yet) found.
 */
size_t VMCodeGenerator::handlerRef(IRHandler* handler)
{
    for (size_t i = 0, e = handlers_.size(); i != e; ++i)
        if (handlers_[i].first == handler->name())
            return i;

    handlers_.push_back(std::make_pair(handler->name(), std::vector<FlowVM::Instruction>()));
    return handlers_.size() - 1;
}

size_t VMCodeGenerator::emit(FlowVM::Instruction instr)
{
    code_.push_back(instr);
    return code_.size() - 1;
}

size_t VMCodeGenerator::allocate(size_t count)
{
    for (size_t i = 0; i < count; ++i)
        allocations_.push_back(true);

    return allocations_.size() - count;
}

void VMCodeGenerator::free(size_t base, size_t count)
{
    for (int i = base, e = base + count; i != e; ++i) {
        allocations_[i] = false;
    }
}

void VMCodeGenerator::visit(AllocaInstr& instr)
{
    size_t count = getConstantInt(instr.operands()[0]);
    size_t base = allocate(count);

    variables_[&instr] = (Register) base;

    printf("alloca %s => r%lu\n", instr.name().c_str(), base);
}

void VMCodeGenerator::visit(ArraySetInstr& instr)
{
    printf("TODO: "); instr.dump();

    assert(dynamic_cast<AllocaInstr*>(instr.array()));
    assert(dynamic_cast<ConstantInt*>(instr.index()));

    // TODO
}

void VMCodeGenerator::visit(StoreInstr& instr)
{
    Value* lhs = instr.variable();
    Value* rhs = instr.expression();

    Register lhsReg = getRegister(lhs);

    // var = int
    if (auto integer = dynamic_cast<ConstantInt*>(rhs)) {
        emit(Opcode::IMOV, lhsReg, integer->get());
        return;
    }

    // TODO var = other types (string, ip, ...)

    // var = var
    auto i = variables_.find(rhs);
    if (i != variables_.end()) {
        emit(Opcode::MOV, lhsReg, i->second);
        return;
    }

    printf("instr:\n");
    instr.dump();
    printf("lhs:\n");
    lhs->dump();
    printf("rhs:\n");
    rhs->dump();
    //assert(!"TODO Following store is not implemented:\n");
}

void VMCodeGenerator::visit(LoadInstr& instr)
{
    printf("LOAD INSTRUCTION: ");
    instr.dump();

    variables_[&instr] = allocate(1);
}

void VMCodeGenerator::visit(CallInstr& instr)
{
    int argc = instr.operands().size();
    Register rbase = allocate(argc);

    // emit call args
    for (int i = 1; i < argc; ++i) {
        Register tmp = getRegister(instr.operands()[i]);
        emit(Opcode::MOV, rbase + i, tmp);
    }

    // emit call
    Register nativeId = 0;//nativeFunction(static_cast<BuiltinFunction*>(callee));
    emit(Opcode::CALL, nativeId, argc, rbase);

    variables_[&instr] = rbase;

    free(rbase + 1, argc - 1);
}

FlowVM::Operand VMCodeGenerator::getConstantInt(Value* value)
{
    if (auto i = dynamic_cast<ConstantInt*>(value))
        return i->get();

    assert(!"Should not happen");
    return 0;
}

FlowVM::Operand VMCodeGenerator::getLabel(BasicBlock* bb)
{
    printf("GET_LABEL: TODO\n");
    return 0; // TODO: compute label (IP) of given BB
}

FlowVM::Operand VMCodeGenerator::getRegister(Value* value)
{
    auto i = variables_.find(value);
    if (i != variables_.end())
        return i->second;

    if (ConstantInt* integer = dynamic_cast<ConstantInt*>(value)) {
        Register reg = allocate(1);
        emit(Opcode::IMOV, reg, integer->get());
        return reg;
    }

    Register reg = allocate(1);
    variables_[value] = reg;

    return reg;
}

void VMCodeGenerator::visit(VmInstr& instr)
{
    switch (operandSignature(instr.opcode())) {
        case InstructionSig::None: {//                   ()
            emit(instr.opcode());
            break;
        }
        case InstructionSig::RRR: { // reg, reg, reg     (ABC)
            Register result = allocate(1);
            Register a = getRegister(instr.operands()[0]);
            Register b = getRegister(instr.operands()[1]);
            emit(instr.opcode(), result, a, b);
            variables_[&instr] = result;
            break;
        }
        case InstructionSig::I: {   // imm16             (A)
            emit(instr.opcode(), getConstantInt(instr.operands()[0]));
            break;
        }
        case InstructionSig::R:    // reg               (A)
            if (resultType(instr.opcode()) == FlowType::Void) {
                emit(instr.opcode(), getRegister(instr.operands()[0]));
            } else {
                Register result = allocate(1);
                variables_[&instr] = result;
                emit(instr.opcode(), result);
            }
            break;
        case InstructionSig::RR:   // reg, reg          (AB)
        case InstructionSig::RI:   // reg, imm16        (AB)
        case InstructionSig::RRI:  // reg, reg, imm16   (ABC)
        case InstructionSig::RII:  // reg, imm16, imm16 (ABC)
        case InstructionSig::RIR:  // reg, imm16, reg   (ABC)
        case InstructionSig::IRR:  // imm16, reg, reg   (ABC)
        case InstructionSig::IIR:  // imm16, imm16, reg (ABC)
            assert(!"TODO");
            break;
    }
}

void VMCodeGenerator::visit(UnaryInstr& instr)
{
    switch (operandSignature(instr.opcode())) {
        case InstructionSig::RR: {
            // reg, reg    (AB)
            Register result = allocate(1);
            Register a = getRegister(instr.operands()[0]);
            emit(instr.opcode(), result, a);
            variables_[&instr] = result;
            break;
        }
        case InstructionSig::RI: {
            Register result = allocate(1);
            Register a = getConstantInt(instr.operands()[0]);
            emit(instr.opcode(), result, a);
            variables_[&instr] = result;
        }
        default: {
            assert(!"Invalid signature for binary operator.");
            break;
        }
    }
}

void VMCodeGenerator::visit(BinaryInstr& instr)
{
    switch (operandSignature(instr.opcode())) {
        case InstructionSig::RRR: {
            // reg, reg, reg    (ABC)
            Register result = allocate(1);
            Register a = getRegister(instr.operands()[0]);
            Register b = getRegister(instr.operands()[1]);
            emit(instr.opcode(), result, a, b);
            variables_[&instr] = result;
            break;
        }
        case InstructionSig::RRI: {
            // reg, reg, imm16  (ABC)
            Register result = allocate(1);
            Register a = getRegister(instr.operands()[0]);
            Register b = getConstantInt(instr.operands()[1]);
            emit(instr.opcode(), result, a, b);
            variables_[&instr] = result;
            break;
        }
        default: {
            assert(!"Invalid signature for binary operator.");
            break;
        }
    }
}

void VMCodeGenerator::visit(PhiNode& instr)
{
    assert(!"Should never reach here, as PHI instruction nodes should have been replaced by target registers.");
}

void VMCodeGenerator::visit(BranchInstr& instr)
{
    printf("TODO: "); instr.dump();

}

void VMCodeGenerator::visit(CondBrInstr& instr)
{
    printf("TODO: "); instr.dump();

}

void VMCodeGenerator::visit(BrInstr& instr)
{
    printf("TODO: "); instr.dump();

    Operand label = getLabel(instr.targetBlock());
    emit(Opcode::JMP, label);
}

void VMCodeGenerator::visit(RetInstr& instr)
{
    if (instr.operands().empty()) {
        emit(Opcode::EXIT);
    } else {
        emit(Opcode::EXIT, getConstantInt(instr.operands()[0]));
    }
}

void VMCodeGenerator::visit(MatchInstr& instr)
{
    printf("TODO: "); instr.dump();

}

} // namespace x0
