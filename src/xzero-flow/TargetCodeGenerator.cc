// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <xzero-flow/TargetCodeGenerator.h>
#include <xzero-flow/vm/Match.h>
#include <xzero-flow/vm/ConstantPool.h>
#include <xzero-flow/vm/Program.h>
#include <xzero-flow/ir/BasicBlock.h>
#include <xzero-flow/ir/ConstantValue.h>
#include <xzero-flow/ir/ConstantArray.h>
#include <xzero-flow/ir/Instructions.h>
#include <xzero-flow/ir/IRProgram.h>
#include <xzero-flow/ir/IRHandler.h>
#include <xzero-flow/ir/IRBuiltinHandler.h>
#include <xzero-flow/ir/IRBuiltinFunction.h>
#include <xzero-flow/FlowType.h>
#include <unordered_map>
#include <limits>
#include <array>

namespace xzero::flow {

using namespace vm;

struct InstructionSignature {
  Opcode opcode;
  const char* const mnemonic;
  OperandSig operandSig;
  StackSig stackSig;

  InstructionSignature(Opcode opc, const char* const m,
                                 OperandSig opsig, StackSig stsig)
      : opcode(opc), mnemonic(m), operandSig(opsig), stackSig(stsig) {}
};

#define SIGDEF(opcode, operandSig, stackSig) \
  { Opcode:: opcode, #opcode, OperandSig:: operandSig, StackSig:: stackSig }

std::vector<InstructionSignature> signatures = {
  // misc
  SIGDEF(NOP,       V,  V_V),
  SIGDEF(DISCARD,   V,  V_X),

  // control
  SIGDEF(EXIT,      I,  V_V),
  SIGDEF(JMP,       I,  V_V),
  SIGDEF(JN,        V,  V_N),
  SIGDEF(JZ,        V,  V_N),

  // numeric
  SIGDEF(ILOAD,     I,  N_V),
  SIGDEF(NLOAD,     N,  N_V),
  SIGDEF(NSTORE,    N,  V_N),
  SIGDEF(NNEG,      V,  N_N),
  SIGDEF(NNOT,      V,  N_N),
  SIGDEF(NADD,      V,  N_NN),
  SIGDEF(NSUB,      V,  N_NN),
  SIGDEF(NMUL,      V,  N_NN),
  SIGDEF(NDIV,      V,  N_NN),
  SIGDEF(NREM,      V,  N_NN),
  SIGDEF(NSHL,      V,  N_NN),
  SIGDEF(NSHR,      V,  N_NN),
  SIGDEF(NPOW,      V,  N_NN),
  SIGDEF(NAND,      V,  N_NN),
  SIGDEF(NOR,       V,  N_NN),
  SIGDEF(NOR,       V,  N_NN),
  SIGDEF(NXOR,      V,  N_NN),
  SIGDEF(NCMPZ,     V,  B_N),
  SIGDEF(NCMPEQ,    V,  B_NN),
  SIGDEF(NCMPNE,    V,  B_NN),
  SIGDEF(NCMPLE,    V,  B_NN),
  SIGDEF(NCMPGE,    V,  B_NN),
  SIGDEF(NCMPLT,    V,  B_NN),
  SIGDEF(NCMPGT,    V,  B_NN),

  // bool
  SIGDEF(BNOT,      V,  B_B),
  SIGDEF(BAND,      V,  B_BB),
  SIGDEF(BOR,       V,  B_BB),
  SIGDEF(BXOR,      V,  B_BB),

  // string
  SIGDEF(SLOAD,     S,  S_V),
  SIGDEF(SSTORE,    s,  V_S),
  SIGDEF(SADD,      V,  S_SS),
  SIGDEF(SADDMULTI, V,  S_V), // TODO
  SIGDEF(SSUBSTR,   V,  S_SNN),
  SIGDEF(SCMPEQ,    V,  B_SS),
  SIGDEF(SCMPNE,    V,  B_SS),
  SIGDEF(SCMPLE,    V,  B_SS),
  SIGDEF(SCMPGE,    V,  B_SS),
  SIGDEF(SCMPLT,    V,  B_SS),
  SIGDEF(SCMPGT,    V,  B_SS),
  SIGDEF(SCMPBEG,   V,  B_SS),
  SIGDEF(SCMPEND,   V,  B_SS),
  SIGDEF(SCONTAINS, V,  B_SS),
  SIGDEF(SLEN,      V,  N_S),
  SIGDEF(SISEMPTY,  V,  B_S),
  SIGDEF(SMATCHEQ,  I,  V_S),
  SIGDEF(SMATCHBEG, I,  V_S),
  SIGDEF(SMATCHEND, I,  V_S),
  SIGDEF(SMATCHR,   I,  V_S),

  // IP
  SIGDEF(PLOAD,     P,  P_V),
  SIGDEF(PSTORE,    p,  V_P),
  SIGDEF(PCMPEQ,    V,  B_PP),
  SIGDEF(PCMPNE,    V,  B_PP),
  SIGDEF(PINCIDR,   V,  B_PC),

  // Cidr
  SIGDEF(CLOAD,     C,  C_V),
  SIGDEF(CSTORE,    c,  V_C),

  // regex
  SIGDEF(SREGMATCH, V,  B_SR),
  SIGDEF(SREGGROUP, V,  S_N),

  SIGDEF(N2S,       V, S_N),
  SIGDEF(P2S,       V, S_P),
  SIGDEF(C2S,       V, S_C),
  SIGDEF(R2S,       V, S_R),
  SIGDEF(S2I,       V, N_S),
  SIGDEF(SURLENC,   V, S_S),
  SIGDEF(SURLDEC,   V, S_S),

  // invokation
  SIGDEF(CALL,      I, X_X),
  SIGDEF(HANDLER,   I, V_X),
};

template <typename T, typename S>
std::vector<T> convert(const std::vector<Constant*>& source) {
  std::vector<T> target(source.size());

  for (size_t i = 0, e = source.size(); i != e; ++i)
    target[i] = static_cast<S*>(source[i])->get();

  return target;
}

TargetCodeGenerator::TargetCodeGenerator()
    : errors_(),
      conditionalJumps_(),
      unconditionalJumps_(),
      handlerId_(0),
      code_(),
      sp_(0),
      stack_(),
      variables_(),
      cp_() {
}

TargetCodeGenerator::~TargetCodeGenerator() {}

std::shared_ptr<Program> TargetCodeGenerator::generate(IRProgram* programIR) {
  for (IRHandler* handler : programIR->handlers())
    generate(handler);

  cp_.setModules(programIR->modules());

  auto program = std::shared_ptr<Program>(new Program(std::move(cp_)));
  program->setup();

  return program;
}

void TargetCodeGenerator::generate(IRHandler* handler) {
  // explicitely forward-declare handler, so we can use its ID internally.
  handlerId_ = handlerRef(handler);

  std::unordered_map<BasicBlock*, size_t> basicBlockEntryPoints;

  // generate code for all basic blocks, sequentially
  for (BasicBlock* bb : handler->basicBlocks()) {
    basicBlockEntryPoints[bb] = getInstructionPointer();
    for (Instr* instr : bb->instructions()) {
      instr->accept(*this);
    }
  }

  // fixiate conditional jump instructions
  for (const auto& target : conditionalJumps_) {
    size_t targetPC = basicBlockEntryPoints[target.first];
    for (const auto& source : target.second) {
      code_[source.pc] =
          makeInstruction(source.opcode, source.condition, targetPC);
    }
  }
  conditionalJumps_.clear();

  // fixiate unconditional jump instructions
  for (const auto& target : unconditionalJumps_) {
    size_t targetPC = basicBlockEntryPoints[target.first];
    for (const auto& source : target.second) {
      code_[source.pc] = makeInstruction(source.opcode, targetPC);
    }
  }
  unconditionalJumps_.clear();

  // fixiate match jump table
  for (const auto& hint : matchHints_) {
    size_t matchId = hint.second;
    MatchInstr* instr = hint.first;
    const auto& cases = instr->cases();
    MatchDef& def = cp_.getMatchDef(matchId);

    for (size_t i = 0, e = cases.size(); i != e; ++i) {
      def.cases[i].pc = basicBlockEntryPoints[cases[i].second];
    }

    if (instr->elseBlock()) {
      def.elsePC = basicBlockEntryPoints[instr->elseBlock()];
    }
  }
  matchHints_.clear();

  cp_.getHandler(handlerId_).second = std::move(code_);

  // cleanup remaining handler-local work vars
  variables_.clear();
}

/**
 * Retrieves the program's handler ID for given handler, possibly
 * forward-declaring given handler if not (yet) found.
 */
size_t TargetCodeGenerator::handlerRef(IRHandler* handler) {
  return cp_.makeHandler(handler->name());
}

size_t TargetCodeGenerator::emitInstr(Instruction instr) {
  code_.push_back(instr);
  return getInstructionPointer() - 1;
}

size_t TargetCodeGenerator::emitCondJump(Opcode opcode,
                                         Value cond,
                                         BasicBlock* bb) {
  size_t pc = emitInstr(Opcode::NOP);
  conditionalJumps_[bb].push_back({pc, opcode, cond});
  return pc;
}

size_t TargetCodeGenerator::emitJump(Opcode opcode, BasicBlock* bb) {
  size_t pc = emitInstr(Opcode::NOP);
  unconditionalJumps_[bb].push_back({pc, opcode});
  return pc;
}

size_t TargetCodeGenerator::emitBinary(Instr& instr, Opcode opcode, Opcode ri) {
  emitLoad(instr.operand(0));
  emitLoad(instr.operand(1));

  return emit(opcode);
}

size_t TargetCodeGenerator::emitBinaryAssoc(Instr& instr, Opcode opcode) {
  // TODO: switch lhs and rhs if lhs is const and rhs is not
  // TODO: revive stack/imm opcodes
  emitLoad(instr.operand(0));
  emitLoad(instr.operand(1));
  return emit(opcode);
}

size_t TargetCodeGenerator::emitUnary(Instr& instr, Opcode opcode) {
  emitLoad(instr.operand(0));
  return emit(opcode);
}

StackPointer TargetCodeGenerator::allocate(const Value* value) {
  stack_.emplace_back(value);
  return stack_.size() - 1;
}

StackPointer TargetCodeGenerator::findOnStack(const Value* value) {
  for (size_t i = 0, e = stack_.size(); i != e; ++i)
    if (stack_[i] == value)
      return i;

  return (StackPointer) -1;
}

void TargetCodeGenerator::discard(const Value* value) {
  assert(stack_.back() == value);
  stack_.pop_back();
}

// {{{ instruction code generation
void TargetCodeGenerator::visit(NopInstr& instr) {
  emit(Opcode::NOP);
}

void TargetCodeGenerator::visit(AllocaInstr& instr) {
  // XXX don't allocate until actually used

  // TODO handle instr.arraySize()
  // emit(Opcode::ALLOCA, getConstantInt(instr.arraySize()));
  // return allocate(instr);
}

size_t TargetCodeGenerator::makeNumber(FlowNumber value) {
  return cp_.makeInteger(value);
}

size_t TargetCodeGenerator::makeNativeHandler(IRBuiltinHandler* builtin) {
  return cp_.makeNativeHandler(builtin->signature().to_s());
}

size_t TargetCodeGenerator::makeNativeFunction(IRBuiltinFunction* builtin) {
  return cp_.makeNativeFunction(builtin->signature().to_s());
}

// variable = expression
void TargetCodeGenerator::visit(StoreInstr& instr) {
  Value* lhs = instr.variable();
  StackPointer di = emit(lhs);
  Value* rhs = instr.expression();

  // const int
  if (auto integer = dynamic_cast<ConstantInt*>(rhs)) {
    FlowNumber number = integer->get();
    if (number <= std::numeric_limits<Operand>::max()) {
      emit(Opcode::ISTORE, di, number);
    } else {
      emit(Opcode::NSTORE, di, cp_.makeInteger(number));
    }
    return;
  }

  // const boolean
  if (auto boolean = dynamic_cast<ConstantBoolean*>(rhs)) {
    emit(Opcode::ISTORE, di, boolean->get());
    return;
  }

  // const string
  if (auto string = dynamic_cast<ConstantString*>(rhs)) {
    emit(Opcode::SSTORE, di, cp_.makeString(string->get()));
    return;
  }

  // const IP address
  if (auto ip = dynamic_cast<ConstantIP*>(rhs)) {
    emit(Opcode::PSTORE, di, cp_.makeIPAddress(ip->get()));
    return;
  }

  // const Cidr
  if (auto cidr = dynamic_cast<ConstantCidr*>(rhs)) {
    emit(Opcode::CSTORE, di, cp_.makeCidr(cidr->get()));
    return;
  }

  // TODO const RegExp
  if (/*auto re =*/dynamic_cast<ConstantRegExp*>(rhs)) {
    assert(!"TODO store const RegExp"); // XXX RSTORE
    return;
  }

  if (auto array = dynamic_cast<ConstantArray*>(rhs)) {
    switch (array->type()) {
      case FlowType::IntArray:
        emit(Opcode::ITSTORE, di,
             cp_.makeIntegerArray(
                 convert<FlowNumber, ConstantInt>(array->get())));
        return;
      case FlowType::StringArray:
        emit(Opcode::STSTORE, di,
             cp_.makeStringArray(
                 convert<std::string, ConstantString>(array->get())));
        return;
      case FlowType::IPAddrArray:
        emit(Opcode::PTSTORE, di,
             cp_.makeIPaddrArray(convert<IPAddress, ConstantIP>(array->get())));
        return;
      case FlowType::CidrArray:
        emit(Opcode::CTSTORE, di,
             cp_.makeCidrArray(convert<Cidr, ConstantCidr>(array->get())));
        return;
      default:
        assert(!"BUG: array");
        abort();
    }
  }

  // var = var
  emit(Opcode::STORE, di, emit(rhs));

  // XXX XXX
  // ---------------------------------------------------
  // var = var
  auto i = variables_.find(rhs);
  if (i != variables_.end()) {
    emit(Opcode::STORE, di, i->second);
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

void TargetCodeGenerator::visit(LoadInstr& instr) {
  emit(instr.variable());
}

size_t TargetCodeGenerator::emitCallArgs(Instr& instr) {
  int argc = instr.operands().size();

  for (int i = 1; i < argc; ++i) {
    emit(instr.operand(i));
  }

  return argc;
}

void TargetCodeGenerator::visit(CallInstr& instr) {
  StackPointer bp = variables_.size() - 1;
  variables_[&instr] = bp;

  emitCallArgs(instr);
  emit(Opcode::CALL, makeNativeFunction(instr.callee()));
}

void TargetCodeGenerator::visit(HandlerCallInstr& instr) {
  emitCallArgs(instr);
  emit(Opcode::HANDLER, makeNativeHandler(instr.callee()));
}

Operand TargetCodeGenerator::getConstantInt(Value* value) {
  assert(dynamic_cast<ConstantInt*>(value) != nullptr && "Must be ConstantInt");
  return static_cast<ConstantInt*>(value)->get();
}

StackPointer TargetCodeGenerator::emitLoad(Value* value) {
  StackPointer sp = stack_.size();
  stack_.emplace_back(value);

  // const int
  if (ConstantInt* integer = dynamic_cast<ConstantInt*>(value)) {
    // FIXME this constant initialization should pretty much be done in the entry block
    FlowNumber number = integer->get();
    if (number <= std::numeric_limits<Operand>::max()) {
      emit(Opcode::ILOAD, number);
    } else {
      emit(Opcode::NLOAD, cp_.makeInteger(number));
    }
    return sp;
  }

  // const boolean
  if (auto boolean = dynamic_cast<ConstantBoolean*>(value)) {
    emit(Opcode::ILOAD, boolean->get());
    return sp;
  }

  // const string
  if (ConstantString* str = dynamic_cast<ConstantString*>(value)) {
    emit(Opcode::SLOAD, cp_.makeString(str->get()));
    return sp;
  }

  // const ip
  if (ConstantIP* ip = dynamic_cast<ConstantIP*>(value)) {
    emit(Opcode::PLOAD, cp_.makeIPAddress(ip->get()));
    return sp;
  }

  // const cidr
  if (ConstantCidr* cidr = dynamic_cast<ConstantCidr*>(value)) {
    emit(Opcode::CLOAD, cp_.makeCidr(cidr->get()));
    return sp;
  }

  // const array<T>
  if (ConstantArray* array = dynamic_cast<ConstantArray*>(value)) {
    StackPointer reg = allocate(1);
    switch (array->type()) {
      case FlowType::IntArray:
        emit(Opcode::ITLOAD, reg,
             cp_.makeIntegerArray(
                 convert<FlowNumber, ConstantInt>(array->get())));
        break;
      case FlowType::StringArray:
        emit(Opcode::STCONST, reg,
             cp_.makeStringArray(
                 convert<std::string, ConstantString>(array->get())));
        break;
      case FlowType::IPAddrArray:
        emit(Opcode::PTCONST, reg,
             cp_.makeIPaddrArray(convert<IPAddress, ConstantIP>(array->get())));
        break;
      case FlowType::CidrArray:
        emit(Opcode::CTCONST, reg,
             cp_.makeCidrArray(convert<Cidr, ConstantCidr>(array->get())));
        break;
      default:
        assert(!"BUG: array");
        abort();
    }
    return reg;
  }

  // const regex
  if (/*ConstantRegExp* re =*/dynamic_cast<ConstantRegExp*>(value)) {
    // emit(Opcode::RCONST, reg, re->id());
    assert(!"TODO: RCONST opcode");
    return sp;
  }

  return sp;
}

void TargetCodeGenerator::visit(PhiNode& instr) {
  assert(!"Should never reach here, as PHI instruction nodes should have been replaced by target registers.");
}

void TargetCodeGenerator::visit(CondBrInstr& instr) {
  if (instr.parent()->isAfter(instr.trueBlock())) {
    emit(Opcode::JZ, emitLoad(instr.condition()), instr.falseBlock());
  } else if (instr.parent()->isAfter(instr.falseBlock())) {
    emit(Opcode::JN, emitLoad(instr.condition()), instr.trueBlock());
  } else {
    emit(Opcode::JN, emitLoad(instr.condition()), instr.trueBlock());
    emit(Opcode::JMP, instr.falseBlock());
  }
}

void TargetCodeGenerator::visit(BrInstr& instr) {
  // to not emit the JMP if the target block is emitted right after this block
  // (and thus, right after this instruction).
  if (instr.parent()->isAfter(instr.targetBlock())) return;

  emit(Opcode::JMP, instr.targetBlock());
}

void TargetCodeGenerator::visit(RetInstr& instr) {
  emit(Opcode::EXIT, getConstantInt(instr.operands()[0]));
}

void TargetCodeGenerator::visit(MatchInstr& instr) {
  static const Opcode ops[] = {[(size_t)MatchClass::Same] = Opcode::SMATCHEQ,
                               [(size_t)MatchClass::Head] = Opcode::SMATCHBEG,
                               [(size_t)MatchClass::Tail] = Opcode::SMATCHEND,
                               [(size_t)MatchClass::RegExp] =
                                   Opcode::SMATCHR, };

  const size_t matchId = cp_.makeMatchDef();
  MatchDef& matchDef = cp_.getMatchDef(matchId);

  matchDef.handlerId = handlerRef(instr.parent()->parent());
  matchDef.op = instr.op();
  matchDef.elsePC = 0;  // XXX to be filled in post-processing the handler

  matchHints_.push_back({&instr, matchId});

  for (const auto& one : instr.cases()) {
    MatchCaseDef caseDef;
    switch (one.first->type()) {
      case FlowType::String:
        caseDef.label =
            cp_.makeString(static_cast<ConstantString*>(one.first)->get());
        break;
      case FlowType::RegExp:
        caseDef.label =
            cp_.makeRegExp(static_cast<ConstantRegExp*>(one.first)->get());
        break;
      default:
        assert(!"BUG: unsupported label type");
        abort();
    }
    caseDef.pc = 0;  // XXX to be filled in post-processing the handler

    matchDef.cases.push_back(caseDef);
  }

  Register condition = emit(instr.condition());

  emit(ops[(size_t)matchDef.op], condition, matchId);
}

void TargetCodeGenerator::visit(CastInstr& instr) {
  // map of (target, source, opcode)
  static const std::unordered_map<
      FlowType,
      std::unordered_map<FlowType, Opcode>> map = {
        {FlowType::String, {
          {FlowType::Number, Opcode::N2S},
          {FlowType::IPAddress, Opcode::P2S},
          {FlowType::Cidr, Opcode::C2S},
          {FlowType::RegExp, Opcode::R2S},
        }},
        {FlowType::Number, {
          {FlowType::String, Opcode::S2I},
        }},
      };

  // just alias same-type casts
  if (instr.type() == instr.source()->type()) {
    variables_[&instr] = emit(instr.source());
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
  Register a = emit(instr.source());
  emit(op, result, a);
}

void TargetCodeGenerator::visit(INegInstr& instr) {
  emitUnary(instr, Opcode::NNEG);
}

void TargetCodeGenerator::visit(INotInstr& instr) {
  emitUnary(instr, Opcode::NNOT);
}

void TargetCodeGenerator::visit(IAddInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NADD);
}

void TargetCodeGenerator::visit(ISubInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NSUB);
}

void TargetCodeGenerator::visit(IMulInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NMUL);
}

void TargetCodeGenerator::visit(IDivInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NDIV);
}

void TargetCodeGenerator::visit(IRemInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NREM);
}

void TargetCodeGenerator::visit(IPowInstr& instr) {
  emitBinary(instr, Opcode::NPOW);
}

void TargetCodeGenerator::visit(IAndInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NAND);
}

void TargetCodeGenerator::visit(IOrInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NOR);
}

void TargetCodeGenerator::visit(IXorInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NXOR);
}

void TargetCodeGenerator::visit(IShlInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NSHL);
}

void TargetCodeGenerator::visit(IShrInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NSHR);
}

void TargetCodeGenerator::visit(ICmpEQInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NCMPEQ);
}

void TargetCodeGenerator::visit(ICmpNEInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NCMPNE);
}

void TargetCodeGenerator::visit(ICmpLEInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NCMPLE);
}

void TargetCodeGenerator::visit(ICmpGEInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NCMPGE);
}

void TargetCodeGenerator::visit(ICmpLTInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NCMPLT);
}

void TargetCodeGenerator::visit(ICmpGTInstr& instr) {
  emitBinaryAssoc(instr, Opcode::NCMPGT);
}

void TargetCodeGenerator::visit(BNotInstr& instr) {
  emitUnary(instr, Opcode::BNOT);
}

void TargetCodeGenerator::visit(BAndInstr& instr) {
  emitBinary(instr, Opcode::BAND);
}

void TargetCodeGenerator::visit(BOrInstr& instr) {
  emitBinary(instr, Opcode::BAND);
}

void TargetCodeGenerator::visit(BXorInstr& instr) {
  emitBinary(instr, Opcode::BXOR);
}

void TargetCodeGenerator::visit(SLenInstr& instr) {
  emitUnary(instr, Opcode::SLEN);
}

void TargetCodeGenerator::visit(SIsEmptyInstr& instr) {
  emitUnary(instr, Opcode::SISEMPTY);
}

void TargetCodeGenerator::visit(SAddInstr& instr) {
  emitBinary(instr, Opcode::SADD);
}

void TargetCodeGenerator::visit(SSubStrInstr& instr) {
  emitBinary(instr, Opcode::SSUBSTR);
}

void TargetCodeGenerator::visit(SCmpEQInstr& instr) {
  emitBinary(instr, Opcode::SCMPEQ);
}

void TargetCodeGenerator::visit(SCmpNEInstr& instr) {
  emitBinary(instr, Opcode::SCMPNE);
}

void TargetCodeGenerator::visit(SCmpLEInstr& instr) {
  emitBinary(instr, Opcode::SCMPLE);
}

void TargetCodeGenerator::visit(SCmpGEInstr& instr) {
  emitBinary(instr, Opcode::SCMPGE);
}

void TargetCodeGenerator::visit(SCmpLTInstr& instr) {
  emitBinary(instr, Opcode::SCMPLT);
}

void TargetCodeGenerator::visit(SCmpGTInstr& instr) {
  emitBinary(instr, Opcode::SCMPGT);
}

void TargetCodeGenerator::visit(SCmpREInstr& instr) {
  assert(dynamic_cast<ConstantRegExp*>(instr.operand(1)) &&
         "RHS must be a ConstantRegExp");

  ConstantRegExp* re = static_cast<ConstantRegExp*>(instr.operand(1));

  emitLoad(instr.operand(0));
  emitLoad(cp_.makeRegExp(re->get()));
  emit(Opcode::SREGMATCH);
}

void TargetCodeGenerator::visit(SCmpBegInstr& instr) {
  emitBinary(instr, Opcode::SCMPBEG);
}

void TargetCodeGenerator::visit(SCmpEndInstr& instr) {
  emitBinary(instr, Opcode::SCMPEND);
}

void TargetCodeGenerator::visit(SInInstr& instr) {
  emitBinary(instr, Opcode::SCONTAINS);
}

void TargetCodeGenerator::visit(PCmpEQInstr& instr) {
  emitBinary(instr, Opcode::PCMPEQ);
}

void TargetCodeGenerator::visit(PCmpNEInstr& instr) {
  emitBinary(instr, Opcode::PCMPNE);
}

void TargetCodeGenerator::visit(PInCidrInstr& instr) {
  emitBinary(instr, Opcode::PINCIDR);
}
// }}}

}  // namespace xzero::flow
