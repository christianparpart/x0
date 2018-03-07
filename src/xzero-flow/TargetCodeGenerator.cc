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
#include <xzero/logging.h>
#include <unordered_map>
#include <limits>
#include <array>
#include <cstdarg>

namespace xzero::flow {

#define FLOW_DEBUG_TCG 1
#if defined(FLOW_DEBUG_TCG)
// {{{ trace
static size_t fni = 0;
struct fntrace4 {
  std::string msg_;

  fntrace4(const char* msg) : msg_(msg) {
    size_t i = 0;
    char fmt[1024];

    for (i = 0; i < 2 * fni;) {
      fmt[i++] = ' ';
      fmt[i++] = ' ';
    }
    fmt[i++] = '-';
    fmt[i++] = '>';
    fmt[i++] = ' ';
    strcpy(fmt + i, msg_.c_str());

    logDebug(fmt);
    ++fni;
  }

  ~fntrace4() {
    --fni;

    size_t i = 0;
    char fmt[1024];

    for (i = 0; i < 2 * fni;) {
      fmt[i++] = ' ';
      fmt[i++] = ' ';
    }
    fmt[i++] = '<';
    fmt[i++] = '-';
    fmt[i++] = ' ';
    strcpy(fmt + i, msg_.c_str());

    logDebug(fmt);
  }
};
// }}}
#define FNTRACE() fntrace4 _(__PRETTY_FUNCTION__)
#define TRACE(level, msg...) logDebug("TCG: " msg)
#else
#define FNTRACE()            do {} while (0)
#define TRACE(level, msg...) do {} while (0)
#endif
using namespace vm;

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
      stack_(),
      cp_() {
}

TargetCodeGenerator::~TargetCodeGenerator() {}

std::unique_ptr<Program> TargetCodeGenerator::generate(IRProgram* programIR) {
  FNTRACE();

  for (IRHandler* handler : programIR->handlers())
    generate(handler);

  cp_.setModules(programIR->modules());

  return std::make_unique<Program>(std::move(cp_));
}

void TargetCodeGenerator::generate(IRHandler* handler) {
  FNTRACE();

  // explicitely forward-declare handler, so we can use its ID internally.
  handlerId_ = cp_.makeHandler(handler);

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
      code_[source.pc] = makeInstruction(source.opcode, targetPC);
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
    MatchInstr* matchInstr = hint.first;
    const auto& cases = matchInstr->cases();
    MatchDef& def = cp_.getMatchDef(matchId);

    for (size_t i = 0, e = cases.size(); i != e; ++i) {
      def.cases[i].pc = basicBlockEntryPoints[cases[i].second];
    }

    if (matchInstr->elseBlock()) {
      def.elsePC = basicBlockEntryPoints[matchInstr->elseBlock()];
    }
  }
  matchHints_.clear();

  cp_.getHandler(handlerId_).second = std::move(code_);

  // cleanup remaining handler-local work vars
  printf("stack depth after run: stackSize=%zu\n", stack_.size());
  stack_.clear();
}

size_t TargetCodeGenerator::emitInstr(Instruction instr) {
  code_.push_back(instr);
  return getInstructionPointer() - 1;
}

size_t TargetCodeGenerator::emitCondJump(Opcode opcode, BasicBlock* bb) {
  size_t pc = emitInstr(opcode);
  conditionalJumps_[bb].push_back({pc, opcode});
  return pc;
}

size_t TargetCodeGenerator::emitJump(BasicBlock* bb) {
  size_t pc = emitInstr(Opcode::JMP);
  unconditionalJumps_[bb].push_back({pc, Opcode::JMP});
  return pc;
}

size_t TargetCodeGenerator::emitBinary(Instr& binaryInstr, Opcode opcode) {
  emitLoad(binaryInstr.operand(0));
  emitLoad(binaryInstr.operand(1));

  return emitInstr(opcode);
}

size_t TargetCodeGenerator::emitBinaryAssoc(Instr& binaryInstr, Opcode opcode) {
  // TODO: switch lhs and rhs if lhs is const and rhs is not
  // TODO: revive stack/imm opcodes
  emitLoad(binaryInstr.operand(0));
  emitLoad(binaryInstr.operand(1));
  return emitInstr(opcode);
}

size_t TargetCodeGenerator::emitUnary(Instr& unaryInstr, Opcode opcode) {
  emitLoad(unaryInstr.operand(0));
  return emitInstr(opcode);
}

StackPointer TargetCodeGenerator::getStackPointer(const Value* value) {
  for (size_t i = 0, e = stack_.size(); i != e; ++i) {
    if (stack_[i] == value) {
      logDebug("getStackPointer: found $0 on stack ($1)", value->name(), i);
      ((Value*) value)->dump();
      return i;
    }
  }

  logDebug("getStackPointer: not found $0 on stack", value->name());
  ((Value*) value)->dump();
  return (StackPointer) -1;
}

void TargetCodeGenerator::changeStack(size_t pops, const Value* pushValue) {
  if (pops)
    pop(pops);

  push(pushValue);
}

void TargetCodeGenerator::pop(size_t count) {
  logDebug("tcg: pop $0 (of $1) values", count, stack_.size());
  if (count > stack_.size())
    logFatal("flow: BUG: stack smaller than amount of elements to pop.");

  for (size_t i = 0; i != count; i++)
    stack_.pop_back();
}

void TargetCodeGenerator::push(const Value* alias) {
  logDebug("tcg: push $0", alias->name());
  stack_.push_back(alias);
}

// {{{ instruction code generation
void TargetCodeGenerator::visit(NopInstr& nopInstr) {
  FNTRACE();
  emitInstr(Opcode::NOP);
}

void TargetCodeGenerator::visit(AllocaInstr& allocaInstr) {
  FNTRACE();

  // TODO handle allocaInstr.arraySize()
  // emitInstr(Opcode::ALLOCA, getConstantInt(allocaInstr.arraySize()));
  // return allocate(allocaInstr);

  stack_.push_back(&allocaInstr);
}

// variable = expression
void TargetCodeGenerator::visit(StoreInstr& storeInstr) {
  FNTRACE();
  emitLoad(storeInstr.expression());

  if (StackPointer di = getStackPointer(storeInstr.variable()); di != size_t(-1)) {
    emitInstr(Opcode::STORE, di);
  } else {
    //DEL variables_[storeInstr.variable()] = getStackPointer() - 1;
  }
}

void TargetCodeGenerator::visit(LoadInstr& loadInstr) {
  FNTRACE();
  StackPointer sp = emitLoad(loadInstr.variable());
  //DEL variables_[&loadInstr] = sp;
}

void TargetCodeGenerator::visit(CallInstr& callInstr) {
  FNTRACE();
  StackPointer bp = getStackPointer();

  int argc = callInstr.operands().size() - 1;
  for (int i = 1; i <= argc; ++i)
    emitLoad(callInstr.operand(i));

  emitInstr(Opcode::CALL,
      cp_.makeNativeFunction(callInstr.callee()),
      callInstr.operands().size() - 1);

  if (argc)
    pop(argc);

  if (callInstr.callee()->signature().returnType() != FlowType::Void)
    push(&callInstr);
}

void TargetCodeGenerator::visit(HandlerCallInstr& handlerCallInstr) {
  FNTRACE();

  int argc = handlerCallInstr.operands().size() - 1;
  for (int i = 1; i <= argc; ++i)
    emitLoad(handlerCallInstr.operand(i));

  emitInstr(Opcode::HANDLER,
      cp_.makeNativeHandler(handlerCallInstr.callee()),
      handlerCallInstr.operands().size() - 1);

  if (argc)
    pop(argc);
}

Operand TargetCodeGenerator::getConstantInt(Value* value) {
  assert(dynamic_cast<ConstantInt*>(value) != nullptr && "Must be ConstantInt");
  return static_cast<ConstantInt*>(value)->get();
}

StackPointer TargetCodeGenerator::emitLoad(Value* value) {
  // if value is already on stack, dup to top
  // if (StackPointer si = getStackPointer(value); si != size_t(-1)) {
  //   logDebug("flow[TargetCodeGenerator]: emitLoad via getStackPointer()");
  //   StackPointer sp = stack_.size();
  //   emitInstr(Opcode::LOAD, si);
  //   return sp;
  // }

  StackPointer sp = getStackPointer();

  // const int
  if (ConstantInt* integer = dynamic_cast<ConstantInt*>(value)) {
    // FIXME this constant initialization should pretty much be done in the entry block
    FlowNumber number = integer->get();
    if (number <= std::numeric_limits<Operand>::max()) {
      emitInstr(Opcode::ILOAD, number);
      changeStack(0, value);
    } else {
      emitInstr(Opcode::NLOAD, cp_.makeInteger(number));
      changeStack(0, value);
    }
    return sp;
  }

  // const boolean
  if (auto boolean = dynamic_cast<ConstantBoolean*>(value)) {
    emitInstr(Opcode::ILOAD, boolean->get());
    changeStack(0, value);
    return sp;
  }

  // const string
  if (ConstantString* str = dynamic_cast<ConstantString*>(value)) {
    emitInstr(Opcode::SLOAD, cp_.makeString(str->get()));
    changeStack(0, value);
    return sp;
  }

  // const ip
  if (ConstantIP* ip = dynamic_cast<ConstantIP*>(value)) {
    emitInstr(Opcode::PLOAD, cp_.makeIPAddress(ip->get()));
    changeStack(0, value);
    return sp;
  }

  // const cidr
  if (ConstantCidr* cidr = dynamic_cast<ConstantCidr*>(value)) {
    emitInstr(Opcode::CLOAD, cp_.makeCidr(cidr->get()));
    changeStack(0, value);
    return sp;
  }

  // const array<T>
  if (ConstantArray* array = dynamic_cast<ConstantArray*>(value)) {
    switch (array->type()) {
      case FlowType::IntArray:
        emitInstr(Opcode::ITLOAD,
             cp_.makeIntegerArray(
                 convert<FlowNumber, ConstantInt>(array->get())));
        changeStack(0, value);
        break;
      case FlowType::StringArray:
        emitInstr(Opcode::STLOAD,
             cp_.makeStringArray(
                 convert<std::string, ConstantString>(array->get())));
        changeStack(0, value);
        break;
      case FlowType::IPAddrArray:
        emitInstr(Opcode::PTLOAD,
             cp_.makeIPaddrArray(convert<IPAddress, ConstantIP>(array->get())));
        changeStack(0, value);
        break;
      case FlowType::CidrArray:
        emitInstr(Opcode::CTLOAD,
             cp_.makeCidrArray(convert<Cidr, ConstantCidr>(array->get())));
        changeStack(0, value);
        break;
      default:
        logFatal("BUG: Unsupported array type in target code generator.");
    }
    return sp;
  }

  // const regex
  if (ConstantRegExp* re = dynamic_cast<ConstantRegExp*>(value)) {
    // TODO emitInstr(Opcode::RLOAD, re->get());
    emitInstr(Opcode::ILOAD, cp_.makeRegExp(re->get()));
    changeStack(0, value);
    return sp;
  }

  value->dump();
  logFatal("BUG: emitLoad() hit with unknown type: $0", typeid(*value).name());
}

void TargetCodeGenerator::visit(PhiNode& phiInstr) {
  FNTRACE();
  logFatal("Should never reach here, as PHI instruction nodes should have been replaced by target registers.");
}

void TargetCodeGenerator::visit(CondBrInstr& condBrInstr) {
  FNTRACE();
  if (condBrInstr.parent()->isAfter(condBrInstr.trueBlock())) {
    emitLoad(condBrInstr.condition());
    emitCondJump(Opcode::JZ, condBrInstr.falseBlock());
  } else if (condBrInstr.parent()->isAfter(condBrInstr.falseBlock())) {
    emitLoad(condBrInstr.condition());
    emitCondJump(Opcode::JN, condBrInstr.trueBlock());
  } else {
    emitLoad(condBrInstr.condition());
    emitCondJump(Opcode::JN, condBrInstr.trueBlock());
    emitJump(condBrInstr.falseBlock());
  }
}

void TargetCodeGenerator::visit(BrInstr& brInstr) {
  // Do not emit the JMP if the target block is emitted right after this block
  // (and thus, right after this instruction).
  if (brInstr.parent()->isAfter(brInstr.targetBlock()))
    return;

  emitJump(brInstr.targetBlock());
}

void TargetCodeGenerator::visit(RetInstr& retInstr) {
  emitInstr(Opcode::EXIT, getConstantInt(retInstr.operands()[0]));
}

void TargetCodeGenerator::visit(MatchInstr& matchInstr) {
  static const Opcode ops[] = {[(size_t)MatchClass::Same] = Opcode::SMATCHEQ,
                               [(size_t)MatchClass::Head] = Opcode::SMATCHBEG,
                               [(size_t)MatchClass::Tail] = Opcode::SMATCHEND,
                               [(size_t)MatchClass::RegExp] = Opcode::SMATCHR, };

  const size_t matchId = cp_.makeMatchDef();
  MatchDef& matchDef = cp_.getMatchDef(matchId);

  matchDef.handlerId = cp_.makeHandler(matchInstr.parent()->parent());
  matchDef.op = matchInstr.op();
  matchDef.elsePC = 0;  // XXX to be filled in post-processing the handler

  matchHints_.push_back({&matchInstr, matchId});

  for (const auto& one : matchInstr.cases()) {
    switch (one.first->type()) {
      case FlowType::String:
        matchDef.cases.emplace_back(
            cp_.makeString(static_cast<ConstantString*>(one.first)->get()));
        break;
      case FlowType::RegExp:
        matchDef.cases.emplace_back(
            cp_.makeRegExp(static_cast<ConstantRegExp*>(one.first)->get()));
        break;
      default:
        logFatal("BUG: unsupported label type");
    }
  }

  emitLoad(matchInstr.condition());
  emitInstr(ops[(size_t)matchDef.op], matchId);
}

void TargetCodeGenerator::visit(CastInstr& castInstr) {
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
          {FlowType::String, Opcode::S2N},
        }},
      };

  // just alias same-type casts
  if (castInstr.type() == castInstr.source()->type()) {
    //DEL variables_[&castInstr] = emitLoad(castInstr.source());
    emitLoad(castInstr.source());
    return;
  }

  // lookup target type
  const auto i = map.find(castInstr.type());
  assert(i != map.end() && "Cast target type not found.");

  // lookup source type
  const auto& sub = i->second;
  auto k = sub.find(castInstr.source()->type());
  assert(k != sub.end() && "Cast source type not found.");
  Opcode op = k->second;

  // emit instruction
  emitLoad(castInstr.source());
  emitInstr(op);
}

void TargetCodeGenerator::visit(INegInstr& ineg) {
  emitUnary(ineg, Opcode::NNEG);
}

void TargetCodeGenerator::visit(INotInstr& inot) {
  emitUnary(inot, Opcode::NNOT);
}

void TargetCodeGenerator::visit(IAddInstr& iadd) {
  emitBinaryAssoc(iadd, Opcode::NADD);
}

void TargetCodeGenerator::visit(ISubInstr& isub) {
  emitBinaryAssoc(isub, Opcode::NSUB);
}

void TargetCodeGenerator::visit(IMulInstr& imul) {
  emitBinaryAssoc(imul, Opcode::NMUL);
}

void TargetCodeGenerator::visit(IDivInstr& idiv) {
  emitBinaryAssoc(idiv, Opcode::NDIV);
}

void TargetCodeGenerator::visit(IRemInstr& irem) {
  emitBinaryAssoc(irem, Opcode::NREM);
}

void TargetCodeGenerator::visit(IPowInstr& ipow) {
  emitBinary(ipow, Opcode::NPOW);
}

void TargetCodeGenerator::visit(IAndInstr& iand) {
  emitBinaryAssoc(iand, Opcode::NAND);
}

void TargetCodeGenerator::visit(IOrInstr& ior) {
  emitBinaryAssoc(ior, Opcode::NOR);
}

void TargetCodeGenerator::visit(IXorInstr& ixor) {
  emitBinaryAssoc(ixor, Opcode::NXOR);
}

void TargetCodeGenerator::visit(IShlInstr& ishl) {
  emitBinaryAssoc(ishl, Opcode::NSHL);
}

void TargetCodeGenerator::visit(IShrInstr& ishr) {
  emitBinaryAssoc(ishr, Opcode::NSHR);
}

void TargetCodeGenerator::visit(ICmpEQInstr& icmpeq) {
  emitBinaryAssoc(icmpeq, Opcode::NCMPEQ);
}

void TargetCodeGenerator::visit(ICmpNEInstr& icmpne) {
  emitBinaryAssoc(icmpne, Opcode::NCMPNE);
}

void TargetCodeGenerator::visit(ICmpLEInstr& icmple) {
  emitBinaryAssoc(icmple, Opcode::NCMPLE);
}

void TargetCodeGenerator::visit(ICmpGEInstr& icmpge) {
  emitBinaryAssoc(icmpge, Opcode::NCMPGE);
}

void TargetCodeGenerator::visit(ICmpLTInstr& icmplt) {
  emitBinaryAssoc(icmplt, Opcode::NCMPLT);
}

void TargetCodeGenerator::visit(ICmpGTInstr& icmpgt) {
  emitBinaryAssoc(icmpgt, Opcode::NCMPGT);
}

void TargetCodeGenerator::visit(BNotInstr& bnot) {
  emitUnary(bnot, Opcode::BNOT);
}

void TargetCodeGenerator::visit(BAndInstr& band) {
  emitBinary(band, Opcode::BAND);
}

void TargetCodeGenerator::visit(BOrInstr& bor) {
  emitBinary(bor, Opcode::BAND);
}

void TargetCodeGenerator::visit(BXorInstr& bxor) {
  emitBinary(bxor, Opcode::BXOR);
}

void TargetCodeGenerator::visit(SLenInstr& slen) {
  emitUnary(slen, Opcode::SLEN);
}

void TargetCodeGenerator::visit(SIsEmptyInstr& sisempty) {
  emitUnary(sisempty, Opcode::SISEMPTY);
}

void TargetCodeGenerator::visit(SAddInstr& sadd) {
  emitBinary(sadd, Opcode::SADD);
}

void TargetCodeGenerator::visit(SSubStrInstr& ssubstr) {
  emitBinary(ssubstr, Opcode::SSUBSTR);
}

void TargetCodeGenerator::visit(SCmpEQInstr& scmpeq) {
  emitBinary(scmpeq, Opcode::SCMPEQ);
}

void TargetCodeGenerator::visit(SCmpNEInstr& scmpne) {
  emitBinary(scmpne, Opcode::SCMPNE);
}

void TargetCodeGenerator::visit(SCmpLEInstr& scmple) {
  emitBinary(scmple, Opcode::SCMPLE);
}

void TargetCodeGenerator::visit(SCmpGEInstr& scmpge) {
  emitBinary(scmpge, Opcode::SCMPGE);
}

void TargetCodeGenerator::visit(SCmpLTInstr& scmplt) {
  emitBinary(scmplt, Opcode::SCMPLT);
}

void TargetCodeGenerator::visit(SCmpGTInstr& scmpgt) {
  emitBinary(scmpgt, Opcode::SCMPGT);
}

void TargetCodeGenerator::visit(SCmpREInstr& scmpre) {
  ConstantRegExp* re = static_cast<ConstantRegExp*>(scmpre.operand(1));
  assert(re != nullptr && "RHS must be a ConstantRegExp");

  emitLoad(scmpre.operand(0));
  emitInstr(Opcode::SREGMATCH, cp_.makeRegExp(re->get()));
}

void TargetCodeGenerator::visit(SCmpBegInstr& scmpbeg) {
  emitBinary(scmpbeg, Opcode::SCMPBEG);
}

void TargetCodeGenerator::visit(SCmpEndInstr& scmpend) {
  emitBinary(scmpend, Opcode::SCMPEND);
}

void TargetCodeGenerator::visit(SInInstr& sin) {
  emitBinary(sin, Opcode::SCONTAINS);
}

void TargetCodeGenerator::visit(PCmpEQInstr& pcmpeq) {
  emitBinary(pcmpeq, Opcode::PCMPEQ);
}

void TargetCodeGenerator::visit(PCmpNEInstr& pcmpne) {
  emitBinary(pcmpne, Opcode::PCMPNE);
}

void TargetCodeGenerator::visit(PInCidrInstr& pincidr) {
  emitBinary(pincidr, Opcode::PINCIDR);
}
// }}}

}  // namespace xzero::flow
