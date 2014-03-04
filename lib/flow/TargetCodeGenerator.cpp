#include <x0/flow/TargetCodeGenerator.h>
#include <x0/flow/vm/Program.h>
#include <x0/flow/ir/BasicBlock.h>
#include <x0/flow/ir/ConstantValue.h>
#include <x0/flow/ir/Instructions.h>
#include <x0/flow/ir/IRProgram.h>
#include <x0/flow/ir/IRHandler.h>
#include <x0/flow/ir/IRBuiltinHandler.h>
#include <x0/flow/ir/IRBuiltinFunction.h>
#include <x0/flow/FlowType.h>
#include <unordered_map>

namespace x0 {

using namespace FlowVM;

TargetCodeGenerator::TargetCodeGenerator() :
    errors_(),
    conditionalJumps_(),
    unconditionalJumps_(),
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

TargetCodeGenerator::~TargetCodeGenerator()
{
}

template<typename T, typename S>
std::vector<T> convert(const std::vector<S>& source)
{
    std::vector<T> target(source.size());

    for (S value: source)
        target[value->id()] = value->get();

    return target;
}

std::unique_ptr<FlowVM::Program> TargetCodeGenerator::generate(IRProgram* program)
{
    for (IRHandler* handler: program->handlers())
        generate(handler);

    std::vector<std::string> strings(convert<std::string>(program->strings()));
    std::vector<IPAddress> ipaddrs(convert<IPAddress>(program->ipaddrs()));
    std::vector<Cidr> cidrs(convert<Cidr>(program->cidrs()));
    std::vector<std::string> regularExpressions(convert<std::string>(program->regularExpressions()));

    return std::unique_ptr<FlowVM::Program>(new FlowVM::Program(
        numbers_,
        strings,
        ipaddrs,
        cidrs,
        regularExpressions,
        matches_,
        program->imports(),
        nativeHandlerSignatures_,
        nativeFunctionSignatures_,
        handlers_
    ));
}

void TargetCodeGenerator::generate(IRHandler* handler)
{
    // explicitely forward-declare handler, so we can use its ID internally.
    handlerId_ = handlerRef(handler);

    std::unordered_map<BasicBlock*, size_t> basicBlockEntryPoints;

    // generate code for all basic blocks, sequentially
    for (BasicBlock* bb: handler->basicBlocks()) {
        basicBlockEntryPoints[bb] = getInstructionPointer();
        for (Instr* instr: bb->instructions()) {
            instr->accept(*this);
        }
    }

    // fixiate conditional jump instructions
    for (const auto& target: conditionalJumps_) {
        size_t targetPC = basicBlockEntryPoints[target.first];
        for (const auto& source: target.second) {
            code_[source.pc] = makeInstruction(source.opcode, source.condition, targetPC);
        }
    }
    conditionalJumps_.clear();

    // fixiate unconditional jump instructions
    for (const auto& target: unconditionalJumps_) {
        size_t targetPC = basicBlockEntryPoints[target.first];
        for (const auto& source: target.second) {
            code_[source.pc] = makeInstruction(source.opcode, targetPC);
        }
    }
    unconditionalJumps_.clear();

    // fixiate match jump table
    for (const auto& hint: matchHints_) {
        size_t matchId = hint.second;
        MatchInstr* instr = hint.first;
        auto cases = instr->cases();

        for (size_t i = 0, e = cases.size(); i != e; ++i) {
            matches_[matchId].cases[i].pc = basicBlockEntryPoints[cases[i].second];
        }

        if (instr->elseBlock()) {
            matches_[matchId].elsePC = basicBlockEntryPoints[instr->elseBlock()];
        }
    }
    matchHints_.clear();

    handlers_[handlerId_].second = std::move(code_);
}

/**
 * Retrieves the program's handler ID for given handler, possibly forward-declaring given handler if not (yet) found.
 */
size_t TargetCodeGenerator::handlerRef(IRHandler* handler)
{
    for (size_t i = 0, e = handlers_.size(); i != e; ++i)
        if (handlers_[i].first == handler->name())
            return i;

    handlers_.push_back(std::make_pair(handler->name(), std::vector<FlowVM::Instruction>()));
    return handlers_.size() - 1;
}

size_t TargetCodeGenerator::emit(FlowVM::Instruction instr)
{
    code_.push_back(instr);
    return getInstructionPointer() - 1;
}

size_t TargetCodeGenerator::emit(FlowVM::Opcode opcode, Register cond, BasicBlock* bb)
{
    size_t pc = emit(Opcode::NOP);
    conditionalJumps_[bb].push_back({pc, opcode, cond});
    return pc;
}

size_t TargetCodeGenerator::emit(FlowVM::Opcode opcode, BasicBlock* bb)
{
    size_t pc = emit(Opcode::NOP);
    unconditionalJumps_[bb].push_back({pc, opcode});
    return pc;
}

size_t TargetCodeGenerator::emitBinary(Instr& instr, Opcode rr)
{
    assert(operandSignature(rr) == InstructionSig::RRR);

    Register a = allocate(1, instr);
    Register b = getRegister(instr.operand(0));
    Register c = getRegister(instr.operand(1));

    return emit(rr, a, b, c);
}

size_t TargetCodeGenerator::emitBinaryAssoc(Instr& instr, Opcode rr, Opcode ri)
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

size_t TargetCodeGenerator::emitBinary(Instr& instr, Opcode rr, Opcode ri)
{
    assert(operandSignature(rr) == InstructionSig::RRR);
    assert(operandSignature(ri) == InstructionSig::RRI);

    Register a = allocate(1, instr);

    if (auto i = dynamic_cast<ConstantInt*>(instr.operand(1))) {
        Register b = getRegister(instr.operand(0));
        return emit(ri, a, b, i->get());
    }

    Register b = getRegister(instr.operand(0));
    Register c = getRegister(instr.operand(1));
    return emit(rr, a, b, c);
}

size_t TargetCodeGenerator::emitUnary(Instr& instr, FlowVM::Opcode r)
{
    assert(operandSignature(r) == InstructionSig::RR);

    Register a = allocate(1, instr);
    Register b = getRegister(instr.operand(0));

    return emit(r, a, b);
}

size_t TargetCodeGenerator::allocate(size_t count, Value& alias)
{
    int rbase = allocate(count);
    variables_[&alias] = rbase;
    return rbase;
}

size_t TargetCodeGenerator::allocate(size_t count)
{
    for (size_t i = 0; i < count; ++i)
        allocations_.push_back(true);

    return allocations_.size() - count;
}

void TargetCodeGenerator::free(size_t base, size_t count)
{
    for (int i = base, e = base + count; i != e; ++i) {
        allocations_[i] = false;
    }
}

// {{{ instruction code generation
void TargetCodeGenerator::visit(NopInstr& instr)
{
    emit(Opcode::NOP);
}

void TargetCodeGenerator::visit(AllocaInstr& instr)
{
    size_t count = getConstantInt(instr.operands()[0]);

    allocate(count, instr);
}

void TargetCodeGenerator::visit(ArraySetInstr& instr)
{
    assert(dynamic_cast<AllocaInstr*>(instr.array()));
    assert(dynamic_cast<ConstantInt*>(instr.index()));

    Operand array = getRegister(instr.array());
    Operand index = getConstantInt(instr.index());

    // array[index] = string_literal;
    if (auto s = dynamic_cast<ConstantString*>(instr.value())) {
        emit(Opcode::ASINIT, array, index, s->id());
        return;
    }

    // array[index] = integer_literal;
    if (auto i = dynamic_cast<ConstantInt*>(instr.value())) {
        emit(Opcode::ANINITI, array, index, i->get());
        return;
    }

    // array[index] = integer_variable;
    if (instr.value()->type() == FlowType::Number) {
        emit(Opcode::ANINIT, array, index, getRegister(instr.value()));
        return;
    };

    assert(!"TODO: missing implementation of ArraySetInstr sub types");
}

size_t TargetCodeGenerator::makeNumber(FlowNumber value)
{
    for (size_t i = 0, e = numbers_.size(); i != e; ++i)
        if (numbers_[i] == value)
            return i;

    numbers_.push_back(value);
    return numbers_.size() - 1;
}

size_t TargetCodeGenerator::makeNativeHandler(IRBuiltinHandler* builtin)
{
    for (size_t i = 0, e = nativeHandlerSignatures_.size(); i != e; ++i)
        if (builtin->signature() == nativeHandlerSignatures_[i])
            return i;

    nativeHandlerSignatures_.push_back(builtin->signature().to_s());
    return nativeHandlerSignatures_.size() - 1;
}

size_t TargetCodeGenerator::makeNativeFunction(IRBuiltinFunction* builtin)
{
    for (size_t i = 0, e = nativeFunctionSignatures_.size(); i != e; ++i)
        if (builtin->signature() == nativeFunctionSignatures_[i])
            return i;

    nativeFunctionSignatures_.push_back(builtin->signature().to_s());
    return nativeFunctionSignatures_.size() - 1;
}

void TargetCodeGenerator::visit(StoreInstr& instr)
{
    Value* lhs = instr.variable();
    Value* rhs = instr.expression();

    Register lhsReg = getRegister(lhs);

    // const int
    if (auto integer = dynamic_cast<ConstantInt*>(rhs)) {
        FlowNumber number = integer->get();
        if (number >= -32768 && number <= 32767) { // limit to 16bit signed width
            emit(Opcode::IMOV, lhsReg, number);
        } else {
            emit(Opcode::NCONST, lhsReg, makeNumber(number));
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

void TargetCodeGenerator::visit(LoadInstr& instr)
{
    // no need to *load* the variable into a register as
    // we have only one variable store
    variables_[&instr] = getRegister(instr.variable());
}

void TargetCodeGenerator::visit(CallInstr& instr)
{
    int argc = instr.operands().size();
    Register rbase = allocate(argc, instr);

    // emit call args
    for (int i = 1; i < argc; ++i) {
        Register tmp = getRegister(instr.operands()[i]);
        emit(Opcode::MOV, rbase + i, tmp);
    }

    // emit call
    Register nativeId = makeNativeFunction(instr.callee());
    emit(Opcode::CALL, nativeId, argc, rbase);

    variables_[&instr] = rbase;

    free(rbase + 1, argc - 1);
}

void TargetCodeGenerator::visit(HandlerCallInstr& instr)
{
    int argc = instr.operands().size();
    Register rbase = allocate(argc, instr);

    // emit call args
    for (int i = 1; i < argc; ++i) {
        Register tmp = getRegister(instr.operands()[i]);
        emit(Opcode::MOV, rbase + i, tmp);
    }

    // emit call
    Register nativeId = makeNativeHandler(instr.callee());
    emit(Opcode::HANDLER, nativeId, argc, rbase);

    variables_[&instr] = rbase;

    free(rbase + 1, argc - 1);
}

FlowVM::Operand TargetCodeGenerator::getConstantInt(Value* value)
{
    if (auto i = dynamic_cast<ConstantInt*>(value))
        return i->get();

    assert(!"Should not happen");
    return 0;
}

FlowVM::Operand TargetCodeGenerator::getRegister(Value* value)
{
    auto i = variables_.find(value);
    if (i != variables_.end())
        return i->second;

    // const int
    if (ConstantInt* integer = dynamic_cast<ConstantInt*>(value)) {
        // FIXME this constant initialization should pretty much be done in the entry block
        Register reg = allocate(1, integer);
        emit(Opcode::IMOV, reg, integer->get());
        return reg;
    }

    // const boolean
    if (auto boolean = dynamic_cast<ConstantBoolean*>(value)) {
        Register reg = allocate(1, boolean);
        emit(Opcode::IMOV, reg, boolean->get());
        return reg;
    }

    // const string
    if (ConstantString* str = dynamic_cast<ConstantString*>(value)) {
        Register reg = allocate(1, str);
        emit(Opcode::SCONST, reg, str->id());
        return reg;
    }

    // const ip
    if (ConstantIP* ip = dynamic_cast<ConstantIP*>(value)) {
        Register reg = allocate(1, ip);
        emit(Opcode::PCONST, reg, ip->id());
        return reg;
    }

    // const cidr
    if (ConstantCidr* cidr = dynamic_cast<ConstantCidr*>(value)) {
        Register reg = allocate(1, cidr);
        emit(Opcode::CCONST, reg, cidr->id());
        return reg;
    }

    // const regex
    if (ConstantRegExp* re = dynamic_cast<ConstantRegExp*>(value)) {
        Register reg = allocate(1, re);
        //emit(Opcode::RCONST, reg, re->id());
        assert(!"TODO: RCONST opcode");
        return reg;
    }

    return allocate(1, value);
}

void TargetCodeGenerator::visit(PhiNode& instr)
{
    assert(!"Should never reach here, as PHI instruction nodes should have been replaced by target registers.");
}

void TargetCodeGenerator::visit(CondBrInstr& instr)
{
    if (instr.parent()->isAfter(instr.trueBlock())) {
        emit(Opcode::JZ, getRegister(instr.condition()), instr.falseBlock());
    }
    else if (instr.parent()->isAfter(instr.falseBlock())) {
        emit(Opcode::JN, getRegister(instr.condition()), instr.trueBlock());
    }
    else {
        emit(Opcode::JN, getRegister(instr.condition()), instr.trueBlock());
        emit(Opcode::JMP, instr.falseBlock());
    }
}

void TargetCodeGenerator::visit(BrInstr& instr)
{
    // to not emit the JMP if the target block is emitted right after this block (and thus, right after this instruction).
    if (instr.parent()->isAfter(instr.targetBlock()))
        return;

    emit(Opcode::JMP, instr.targetBlock());
}

void TargetCodeGenerator::visit(RetInstr& instr)
{
    emit(Opcode::EXIT, getConstantInt(instr.operands()[0]));
}

void TargetCodeGenerator::visit(MatchInstr& instr)
{
    static const Opcode ops[] = {
        [(size_t) MatchClass::Same] = Opcode::SMATCHEQ,
        [(size_t) MatchClass::Head] = Opcode::SMATCHBEG,
        [(size_t) MatchClass::Tail] = Opcode::SMATCHEND,
        [(size_t) MatchClass::RegExp] = Opcode::SMATCHR,
    };

    const size_t matchId = matches_.size();

    MatchDef matchDef;
    matchDef.handlerId = handlerRef(instr.parent()->parent());
    matchDef.op = instr.op();
    matchDef.elsePC = 0; // XXX to be filled in post-processing the handler

    matchHints_.push_back({&instr, matchId});

    for (const auto& one: instr.cases()) {
        MatchCaseDef caseDef;
        caseDef.label = static_cast<Constant*>(one.first)->id();
        caseDef.pc = 0; // XXX to be filled in post-processing the handler

        matchDef.cases.push_back(caseDef);
    }

    Register condition = getRegister(instr.condition());

    emit(ops[(size_t) matchDef.op], condition, matchId);

    matches_.push_back(std::move(matchDef));
}

void TargetCodeGenerator::visit(CastInstr& instr)
{
    // map of (target, source, opcode)
    static const std::unordered_map<FlowType, std::unordered_map<FlowType, Opcode>> map = {
        { FlowType::String, {
            { FlowType::Number, Opcode::I2S },
            { FlowType::IPAddress, Opcode::P2S },
            { FlowType::Cidr, Opcode::C2S },
            { FlowType::RegExp, Opcode::R2S },
        }},
        { FlowType::Number, {
            { FlowType::String, Opcode::I2S },
        }},
    };

    // just alias same-type casts
    if (instr.type() == instr.source()->type()) {
        variables_[&instr] = getRegister(instr.source());
        return;
    }

    // lookup target type
    const auto i = map.find(instr.type());
    assert(i != map.end() && "Cast target type not found.");

    // lookup source type
    const auto& sub = i->second;
    auto k = sub.find(instr.source()->type());
    assert(k != sub.end() && "Cast source type not found.");
    Opcode op = k->second;

    // emit instruction
    Register result = allocate(1, instr);
    Register a = getRegister(instr.source());
    emit(op, result, a);
}

void TargetCodeGenerator::visit(INegInstr& instr)
{
    emitUnary(instr, Opcode::NNEG);
}

void TargetCodeGenerator::visit(INotInstr& instr)
{
    assert(!"TODO: ~ operator not yet implemented in VM");
    //Register a = allocate(1, instr);
    //Register b = getRegister(instr.operands()[0]);
    //emit(Opcode::NNOT, a, b);
}

void TargetCodeGenerator::visit(IAddInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NADD, Opcode::NIADD);
}

void TargetCodeGenerator::visit(ISubInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NSUB, Opcode::NISUB);
}

void TargetCodeGenerator::visit(IMulInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NMUL, Opcode::NIMUL);
}

void TargetCodeGenerator::visit(IDivInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NDIV, Opcode::NIDIV);
}

void TargetCodeGenerator::visit(IRemInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NREM, Opcode::NIREM);
}

void TargetCodeGenerator::visit(IPowInstr& instr)
{
    emitBinary(instr, Opcode::NPOW, Opcode::NIPOW);
}

void TargetCodeGenerator::visit(IAndInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NAND, Opcode::NIAND);
}

void TargetCodeGenerator::visit(IOrInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NOR, Opcode::NIOR);
}

void TargetCodeGenerator::visit(IXorInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NXOR, Opcode::NIXOR);
}

void TargetCodeGenerator::visit(IShlInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NSHL, Opcode::NISHL);
}

void TargetCodeGenerator::visit(IShrInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NSHR, Opcode::NISHR);
}

void TargetCodeGenerator::visit(ICmpEQInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPEQ, Opcode::NICMPEQ);
}

void TargetCodeGenerator::visit(ICmpNEInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPNE, Opcode::NICMPNE);
}

void TargetCodeGenerator::visit(ICmpLEInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPLE, Opcode::NICMPLE);
}

void TargetCodeGenerator::visit(ICmpGEInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPGE, Opcode::NICMPGE);
}

void TargetCodeGenerator::visit(ICmpLTInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPLT, Opcode::NICMPLT);
}

void TargetCodeGenerator::visit(ICmpGTInstr& instr)
{
    emitBinaryAssoc(instr, Opcode::NCMPGT, Opcode::NICMPGT);
}

void TargetCodeGenerator::visit(BNotInstr& instr)
{
    emitUnary(instr, Opcode::BNOT);
}

void TargetCodeGenerator::visit(BAndInstr& instr)
{
    emitBinary(instr, Opcode::BAND);
}

void TargetCodeGenerator::visit(BOrInstr& instr)
{
    emitBinary(instr, Opcode::BOR);
}

void TargetCodeGenerator::visit(BXorInstr& instr)
{
    emitBinary(instr, Opcode::BXOR);
}

void TargetCodeGenerator::visit(SLenInstr& instr)
{
    assert(!"TODO: SLenInstr CG");
}

void TargetCodeGenerator::visit(SIsEmptyInstr& instr)
{
    assert(!"TODO: SIsEmptyInstr CG");
}

void TargetCodeGenerator::visit(SAddInstr& instr)
{
    emitBinary(instr, Opcode::SADD);
}

void TargetCodeGenerator::visit(SSubStrInstr& instr)
{
    assert(!"TODO: SSubStrInstr CG");
}

void TargetCodeGenerator::visit(SCmpEQInstr& instr)
{
    emitBinary(instr, Opcode::SCMPEQ);
}

void TargetCodeGenerator::visit(SCmpNEInstr& instr)
{
    emitBinary(instr, Opcode::SCMPNE);
}

void TargetCodeGenerator::visit(SCmpLEInstr& instr)
{
    emitBinary(instr, Opcode::SCMPLE);
}

void TargetCodeGenerator::visit(SCmpGEInstr& instr)
{
    emitBinary(instr, Opcode::SCMPGE);
}

void TargetCodeGenerator::visit(SCmpLTInstr& instr)
{
    emitBinary(instr, Opcode::SCMPLT);
}

void TargetCodeGenerator::visit(SCmpGTInstr& instr)
{
    emitBinary(instr, Opcode::SCMPGT);
}

void TargetCodeGenerator::visit(SCmpREInstr& instr)
{
    emitBinary(instr, Opcode::SREGMATCH);
}

void TargetCodeGenerator::visit(SCmpBegInstr& instr)
{
    emitBinary(instr, Opcode::SCMPBEG);
}

void TargetCodeGenerator::visit(SCmpEndInstr& instr)
{
    emitBinary(instr, Opcode::SCMPEND);
}

void TargetCodeGenerator::visit(SInInstr& instr)
{
    emitBinary(instr, Opcode::SCONTAINS);
}

void TargetCodeGenerator::visit(PCmpEQInstr& instr)
{
    emitBinary(instr, Opcode::PCMPEQ);
}

void TargetCodeGenerator::visit(PCmpNEInstr& instr)
{
    emitBinary(instr, Opcode::PCMPNE);
}

void TargetCodeGenerator::visit(PInCidrInstr& instr)
{
    emitBinary(instr, Opcode::PINCIDR);
}
// }}}

} // namespace x0
