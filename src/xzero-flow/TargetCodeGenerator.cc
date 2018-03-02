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

//#define FLOW_DEBUG_TCG 1
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

    logDebug("TCG", fmt);
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

    logDebug("TCG", fmt);
  }
};
// }}}
#define FNTRACE() fntrace4 _(__PRETTY_FUNCTION__)
#define TRACE(level, msg...) XZERO_DEBUG("TCG", (level), msg)
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
      sp_(0),
      stack_(),
      variables_(),
      cp_() {
}

TargetCodeGenerator::~TargetCodeGenerator() {}

std::unique_ptr<Program> TargetCodeGenerator::generate(IRProgram* programIR) {
  for (IRHandler* handler : programIR->handlers())
    generate(handler);

  cp_.setModules(programIR->modules());

  return std::make_unique<Program>(std::move(cp_));
}

void TargetCodeGenerator::generate(IRHandler* handler) {
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
  stack_.clear();
  sp_ = 0;
}

size_t TargetCodeGenerator::emitInstr(Instruction instr) {
  code_.push_back(instr);
  sp_ += getStackChange(instr);
  return getInstructionPointer() - 1;
}

size_t TargetCodeGenerator::emitCondJump(Opcode opcode,
                                         BasicBlock* bb) {
  size_t pc = emitInstr(opcode);
  conditionalJumps_[bb].push_back({pc, opcode});
  return pc;
}

size_t TargetCodeGenerator::emitJump(BasicBlock* bb) {
  size_t pc = emitInstr(Opcode::JMP);
  unconditionalJumps_[bb].push_back({pc, Opcode::JMP});
  return pc;
}

size_t TargetCodeGenerator::emitBinary(Instr& instr, Opcode opcode) {
  emitLoad(instr.operand(0));
  emitLoad(instr.operand(1));

  return emitInstr(opcode);
}

size_t TargetCodeGenerator::emitBinaryAssoc(Instr& instr, Opcode opcode) {
  // TODO: switch lhs and rhs if lhs is const and rhs is not
  // TODO: revive stack/imm opcodes
  emitLoad(instr.operand(0));
  emitLoad(instr.operand(1));
  return emitInstr(opcode);
}

size_t TargetCodeGenerator::emitUnary(Instr& instr, Opcode opcode) {
  emitLoad(instr.operand(0));
  return emitInstr(opcode);
}

StackPointer TargetCodeGenerator::getStackPointer(const Value* value) {
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
  FNTRACE();
  emitInstr(Opcode::NOP);
}

void TargetCodeGenerator::visit(AllocaInstr& instr) {
  FNTRACE();

  // TODO handle instr.arraySize()
  // emitInstr(Opcode::ALLOCA, getConstantInt(instr.arraySize()));
  // return allocate(instr);

  variables_[&instr] = sp_++;
}

// variable = expression
void TargetCodeGenerator::visit(StoreInstr& instr) {
  FNTRACE();
  emitLoad(instr.expression());

  if (StackPointer di = getStackPointer(instr.variable()); di != size_t(-1)) {
    emitInstr(Opcode::STORE, di);
  } else {
    variables_[instr.variable()] = getStackPointer() - 1;
  }
}

void TargetCodeGenerator::visit(LoadInstr& instr) {
  FNTRACE();
  StackPointer sp = emitLoad(instr.variable());
  variables_[&instr] = sp;
}

void TargetCodeGenerator::emitCallArgs(Instr& instr) {
  int argc = instr.operands().size();

  // operand(0) skipped. as this one is passed as operand (not stack)

  for (int i = 1; i < argc; ++i) {
    emitLoad(instr.operand(i));
  }
}

void TargetCodeGenerator::visit(CallInstr& instr) {
  FNTRACE();
  StackPointer bp = variables_.size() - 1;
  variables_[&instr] = bp;

  emitCallArgs(instr);
  emitInstr(Opcode::CALL,
      cp_.makeNativeFunction(instr.callee()),
      instr.operands().size() - 1);
}

void TargetCodeGenerator::visit(HandlerCallInstr& instr) {
  FNTRACE();
  emitCallArgs(instr);
  emitInstr(Opcode::HANDLER,
      cp_.makeNativeHandler(instr.callee()),
      instr.operands().size() - 1);
}

Operand TargetCodeGenerator::getConstantInt(Value* value) {
  assert(dynamic_cast<ConstantInt*>(value) != nullptr && "Must be ConstantInt");
  return static_cast<ConstantInt*>(value)->get();
}

StackPointer TargetCodeGenerator::emitLoad(Value* value) {
  // if value is already on stack, dup to top
  if (StackPointer si = getStackPointer(value); si != size_t(-1)) {
    StackPointer sp = stack_.size();
    emitInstr(Opcode::LOAD, si);
    return sp;
  }

  if (auto i = variables_.find(value); i != variables_.end()) {
    StackPointer si = i->second;
    //emitInstr(Opcode::LOAD, si);
    return si;//i->second;
  }

  StackPointer sp = stack_.size();
  stack_.emplace_back(value);

  // const int
  if (ConstantInt* integer = dynamic_cast<ConstantInt*>(value)) {
    // FIXME this constant initialization should pretty much be done in the entry block
    FlowNumber number = integer->get();
    if (number <= std::numeric_limits<Operand>::max()) {
      emitInstr(Opcode::ILOAD, number);
    } else {
      emitInstr(Opcode::NLOAD, cp_.makeInteger(number));
    }
    return sp;
  }

  // const boolean
  if (auto boolean = dynamic_cast<ConstantBoolean*>(value)) {
    emitInstr(Opcode::ILOAD, boolean->get());
    return sp;
  }

  // const string
  if (ConstantString* str = dynamic_cast<ConstantString*>(value)) {
    emitInstr(Opcode::SLOAD, cp_.makeString(str->get()));
    return sp;
  }

  // const ip
  if (ConstantIP* ip = dynamic_cast<ConstantIP*>(value)) {
    emitInstr(Opcode::PLOAD, cp_.makeIPAddress(ip->get()));
    return sp;
  }

  // const cidr
  if (ConstantCidr* cidr = dynamic_cast<ConstantCidr*>(value)) {
    emitInstr(Opcode::CLOAD, cp_.makeCidr(cidr->get()));
    return sp;
  }

  // const array<T>
  if (ConstantArray* array = dynamic_cast<ConstantArray*>(value)) {
    switch (array->type()) {
      case FlowType::IntArray:
        emitInstr(Opcode::ITLOAD,
             cp_.makeIntegerArray(
                 convert<FlowNumber, ConstantInt>(array->get())));
        break;
      case FlowType::StringArray:
        emitInstr(Opcode::STLOAD,
             cp_.makeStringArray(
                 convert<std::string, ConstantString>(array->get())));
        break;
      case FlowType::IPAddrArray:
        emitInstr(Opcode::PTLOAD,
             cp_.makeIPaddrArray(convert<IPAddress, ConstantIP>(array->get())));
        break;
      case FlowType::CidrArray:
        emitInstr(Opcode::CTLOAD,
             cp_.makeCidrArray(convert<Cidr, ConstantCidr>(array->get())));
        break;
      default:
        assert(!"BUG: Unsupported array type");
        abort();
    }
    return sp;
  }

  // const regex
  if (ConstantRegExp* re = dynamic_cast<ConstantRegExp*>(value)) {
    // TODO emitInstr(Opcode::RLOAD, re->get());
    emitInstr(Opcode::ILOAD, cp_.makeRegExp(re->get()));
    return sp;
  }

  printf("BUG: emitLoad() hit unknown type: %s\n", typeid(*value).name());

  assert(!"BUG: emitLoad() for unknown Value* type");
  abort();
}

void TargetCodeGenerator::visit(PhiNode& instr) {
  FNTRACE();
  assert(!"Should never reach here, as PHI instruction nodes should have been replaced by target registers.");
}

void TargetCodeGenerator::visit(CondBrInstr& instr) {
  FNTRACE();
  if (instr.parent()->isAfter(instr.trueBlock())) {
    emitLoad(instr.condition());
    emitCondJump(Opcode::JZ, instr.falseBlock());
  } else if (instr.parent()->isAfter(instr.falseBlock())) {
    emitLoad(instr.condition());
    emitCondJump(Opcode::JN, instr.trueBlock());
  } else {
    emitLoad(instr.condition());
    emitCondJump(Opcode::JN, instr.trueBlock());
    emitJump(instr.falseBlock());
  }
}

void TargetCodeGenerator::visit(BrInstr& instr) {
  // Do not emit the JMP if the target block is emitted right after this block
  // (and thus, right after this instruction).
  if (instr.parent()->isAfter(instr.targetBlock()))
    return;

  emitJump(instr.targetBlock());
}

void TargetCodeGenerator::visit(RetInstr& instr) {
  emitInstr(Opcode::EXIT, getConstantInt(instr.operands()[0]));
}

void TargetCodeGenerator::visit(MatchInstr& instr) {
  static const Opcode ops[] = {[(size_t)MatchClass::Same] = Opcode::SMATCHEQ,
                               [(size_t)MatchClass::Head] = Opcode::SMATCHBEG,
                               [(size_t)MatchClass::Tail] = Opcode::SMATCHEND,
                               [(size_t)MatchClass::RegExp] = Opcode::SMATCHR, };

  const size_t matchId = cp_.makeMatchDef();
  MatchDef& matchDef = cp_.getMatchDef(matchId);

  matchDef.handlerId = cp_.makeHandler(instr.parent()->parent());
  matchDef.op = instr.op();
  matchDef.elsePC = 0;  // XXX to be filled in post-processing the handler

  matchHints_.push_back({&instr, matchId});

  for (const auto& one : instr.cases()) {
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
        assert(!"BUG: unsupported label type");
        abort();
    }
  }

  emitLoad(instr.condition());
  emitInstr(ops[(size_t)matchDef.op], matchId);
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
          {FlowType::String, Opcode::S2N},
        }},
      };

  // just alias same-type casts
  if (instr.type() == instr.source()->type()) {
    variables_[&instr] = emitLoad(instr.source());
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
  emitLoad(instr.source());
  emitInstr(op);
}

void TargetCodeGenerator::visit(INegInstr& instr) {
  emitUnary(instr, Opcode::NNEG);
}

void TargetCodeGenerator::visit(INotInstr& instr) {
  emitUnary(instr, Opcode::NNOT);
}

void TargetCodeGenerator::visit(IAddInstr& instr) {
  variables_[&instr] = getStackPointer();
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
  ConstantRegExp* re = static_cast<ConstantRegExp*>(instr.operand(1));
  assert(re != nullptr && "RHS must be a ConstantRegExp");

  emitLoad(instr.operand(0));
  emitInstr(Opcode::SREGMATCH, cp_.makeRegExp(re->get()));
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
