#include <x0/flow/VMCodeGenerator.h>
#include <x0/flow/vm/Program.h>
#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/ConstantValue.h>
#include <x0/flow/ir/Instructions.h>
#include <x0/flow/ir/IRProgram.h>
#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/IRBuiltinHandler.h>
#include <x0/flow/ir/IRBuiltinFunction.h>

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

size_t VMCodeGenerator::emitBinaryAssoc(Instr& instr, Opcode rr, Opcode ri)
{
    assert(operandSignature(rr) == InstructionSig::RRR);
    assert(operandSignature(ri) == InstructionSig::RRI);

    Register a = allocate(1, instr);

    if (auto i = dynamic_cast<ConstantInt*>(instr.operand(1))) {
        Register b = getRegister(instr.operand(0));
        return emit(ri, a, b, i->get());
    }

    if (auto i = dynamic_cast<ConstantInt*>(instr.operand(0))) {
        Register b = getRegister(instr.operand(1));
        return emit(ri, a, b, i->get());
    }

    Register b = getRegister(instr.operand(0));
    Register c = getRegister(instr.operand(1));
    return emit(rr, a, b, c);
}

size_t VMCodeGenerator::emitBinaryAssoc(Instr& instr, Opcode rr)
{
    assert(operandSignature(rr) == InstructionSig::RRR);

    Register a = allocate(1, instr);
    Register b = getRegister(instr.operand(0));
    Register c = getRegister(instr.operand(1));

    return emit(rr, a, b, c);
}

size_t VMCodeGenerator::emitUnary(Instr& instr, FlowVM::Opcode r)
{
    assert(operandSignature(r) == InstructionSig::RR);

    Register a = allocate(1, instr);
    Register b = getRegister(instr.operand(0));

    return emit(r, a, b);
}

size_t VMCodeGenerator::allocate(size_t count, Value& alias)
{
    int rbase = allocate(count);
    variables_[&alias] = rbase;
    return rbase;
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
    size_t base = allocate(count, instr);

    printf("alloca %s => r%lu\n", instr.name().c_str(), base);
}

void VMCodeGenerator::visit(ArraySetInstr& instr)
{
    printf("TODO: "); instr.dump();

    assert(dynamic_cast<AllocaInstr*>(instr.array()));
    assert(dynamic_cast<ConstantInt*>(instr.index()));

    Operand array = getRegister(instr.array());
    Operand index = getConstantInt(instr.index());

    if (auto i = dynamic_cast<ConstantInt*>(instr.value())) {
        emit(Opcode::ANINITI, array, index, i->get());
        return;
    }

    if (auto s = dynamic_cast<ConstantString*>(instr.value())) {
        emit(Opcode::ASINIT, array, index, s->id());
        return;
    }

    // TODO
}

void VMCodeGenerator::visit(StoreInstr& instr)
{
    Value* lhs = instr.variable();
    Value* rhs = instr.expression();

    Register lhsReg = getRegister(lhs);

    // const int
    if (auto integer = dynamic_cast<ConstantInt*>(rhs)) {
        if (integer->get() >= -32768 && integer->get() <= 32767) { // limit to 16bit signed width
            emit(Opcode::IMOV, lhsReg, integer->get());
        } else {
            emit(Opcode::NCONST, lhsReg, integer->id());
        }
        return;
    }

    // const boolean
    if (auto boolean = dynamic_cast<ConstantBoolean*>(rhs)) {
        emit(Opcode::IMOV, lhsReg, boolean->get());
        return;
    }

    // const string
    if (auto string = dynamic_cast<ConstantString*>(rhs)) {
        emit(Opcode::SCONST, lhsReg, string->id());
        return;
    }

    // const IP address
    if (auto ip = dynamic_cast<ConstantIP*>(rhs)) {
        emit(Opcode::PCONST, lhsReg, ip->id());
        return;
    }

    // const Cidr
    if (auto cidr = dynamic_cast<ConstantCidr*>(rhs)) {
        emit(Opcode::CCONST, lhsReg, cidr->id());
        return;
    }

    // TODO const RegExp
    if (/*auto re =*/ dynamic_cast<ConstantRegExp*>(rhs)) {
        assert(!"TODO store const RegExp");
        return;
    }

    // var = var
    auto i = variables_.find(rhs);
    if (i != variables_.end()) {
        emit(Opcode::MOV, lhsReg, i->second);
        return;
    }

#ifndef NDEBUG
    printf("instr:\n");
    instr.dump();
    printf("lhs:\n");
    lhs->dump();
    printf("rhs:\n");
    rhs->dump();
#endif
    assert(!"Store variant not implemented!");
}

void VMCodeGenerator::visit(LoadInstr& instr)
{
    // no need to *load* the variable into a register as
    // we have only one variable store
    variables_[&instr] = getRegister(instr.variable());
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
    Register nativeId = 0;// TODO nativeFunction(static_cast<BuiltinFunction*>(callee));
    emit(Opcode::CALL, nativeId, argc, rbase);

    variables_[&instr] = rbase;

    free(rbase + 1, argc - 1);
}

void VMCodeGenerator::visit(HandlerCallInstr& instr)
{
    assert(!"TODO");
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
        Register reg = allocate(1, integer);
        emit(Opcode::IMOV, reg, integer->get());
        return reg;
    }

    // TODO: other const-to-reg implicits (string, ip, cidr, regex)

    return allocate(1, value);
}

void VMCodeGenerator::visit(PhiNode& instr)
{
    assert(!"Should never reach here, as PHI instruction nodes should have been replaced by target registers.");
}

void VMCodeGenerator::visit(CondBrInstr& instr)
{
    // ensure that trueBlock is straight-line after the current one
    //instr.parent()->moveAfter(instr.trueBlock());
    assert(instr.parent()->isAfter(instr.trueBlock()));

    auto condition = getRegister(instr.condition());
    auto falseLabel = getLabel(instr.falseBlock());

    emit(Opcode::JZ, condition, falseLabel);
}

void VMCodeGenerator::visit(BrInstr& instr)
{
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

void VMCodeGenerator::visit(CastInstr& instr)
{
    if (instr.type() == FlowType::Number) {
        assert(instr.source()->type() == FlowType::String);
        Register result = allocate(1, instr);
        Register a = getRegister(instr.source());
        emit(Opcode::S2I, result, a);
        return;
    }

    assert(!"Cast combination not implemented.");
}

void VMCodeGenerator::visit(INegInstr& instr)
{
    Register result = allocate(1, instr);
    Register a = getRegister(instr.operands()[0]);
    emit(Opcode::NNEG, result, a);
}

void VMCodeGenerator::visit(INotInstr& instr)
{
    assert(!"~ operator not yet implemented in VM");
    //Register a = allocate(1, instr);
    //Register b = getRegister(instr.operands()[0]);
    //emit(Opcode::NNOT, a, b);
}

void VMCodeGenerator::visit(IAddInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NADD, Opcode::NIADD);
}

void VMCodeGenerator::visit(ISubInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NSUB, Opcode::NISUB);
}

void VMCodeGenerator::visit(IMulInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NMUL, Opcode::NIMUL);
}

void VMCodeGenerator::visit(IDivInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NDIV, Opcode::NIDIV);
}

void VMCodeGenerator::visit(IRemInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NREM, Opcode::NIREM);
}

void VMCodeGenerator::visit(IPowInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NPOW);
}

void VMCodeGenerator::visit(IAndInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NAND, Opcode::NIAND);
}

void VMCodeGenerator::visit(IOrInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NOR, Opcode::NIOR);
}

void VMCodeGenerator::visit(IXorInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NXOR, Opcode::NIXOR);
}

void VMCodeGenerator::visit(IShlInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NSHL, Opcode::NISHL);
}

void VMCodeGenerator::visit(IShrInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NSHR, Opcode::NISHR);
}

void VMCodeGenerator::visit(ICmpEQInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPEQ, Opcode::NICMPEQ);
}

void VMCodeGenerator::visit(ICmpNEInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPNE, Opcode::NICMPNE);
}

void VMCodeGenerator::visit(ICmpLEInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPLE, Opcode::NICMPLE);
}

void VMCodeGenerator::visit(ICmpGEInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPGE, Opcode::NICMPGE);
}

void VMCodeGenerator::visit(ICmpLTInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPLT, Opcode::NICMPLT);
}

void VMCodeGenerator::visit(ICmpGTInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPGT, Opcode::NICMPGT);
}

void VMCodeGenerator::visit(BNotInstr& instr)
{
    emitUnary(instr, Opcode::BNOT);
}

void VMCodeGenerator::visit(BAndInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::BAND);
}

void VMCodeGenerator::visit(BOrInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::BOR);
}

void VMCodeGenerator::visit(BXorInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::BXOR);
}

void VMCodeGenerator::visit(SLenInstr& instr)
{
}

void VMCodeGenerator::visit(SIsEmptyInstr& instr)
{
}

void VMCodeGenerator::visit(SAddInstr& instr)
{
}

void VMCodeGenerator::visit(SSubStrInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpEQInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpNEInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpLEInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpGEInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpLTInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpGTInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpREInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpBegInstr& instr)
{
}

void VMCodeGenerator::visit(SCmpEndInstr& instr)
{
}

void VMCodeGenerator::visit(SInInstr& instr)
{
}

#if 0
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
#endif

} // namespace x0
