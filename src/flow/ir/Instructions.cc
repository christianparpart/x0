// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/BasicBlock.h>
#include <flow/ir/Instructions.h>
#include <flow/ir/InstructionVisitor.h>
#include <flow/ir/IRBuiltinHandler.h>
#include <flow/ir/IRBuiltinFunction.h>
#include <utility>  // make_pair
#include <assert.h>

namespace xzero::flow {

template <typename T, typename U>
inline std::vector<U> join(const T& a, const std::vector<U>& vec)  // {{{
{
  std::vector<U> res;

  res.push_back(a);
  for (const U& v : vec) res.push_back(v);

  return std::move(res);
}
// }}}
const char* cstr(UnaryOperator op)  // {{{
{
  switch (op) {
  case UnaryOperator::INeg: return "ineg";
  case UnaryOperator::INot: return "inot";
  case UnaryOperator::BNot: return "bnot";
  case UnaryOperator::SLen: return "slen";
  case UnaryOperator::SIsEmpty: return "sisempty";
  }
}
// }}}
const char* cstr(BinaryOperator op)  // {{{
{
  switch (op) {
    // numerical
    case BinaryOperator::IAdd: return"iadd";
    case BinaryOperator::ISub: return"isub";
    case BinaryOperator::IMul: return"imul";
    case BinaryOperator::IDiv: return"idiv";
    case BinaryOperator::IRem: return "irem";
    case BinaryOperator::IPow: return "ipow";
    case BinaryOperator::IAnd: return "iand";
    case BinaryOperator::IOr: return "ior";
    case BinaryOperator::IXor: return "ixor";
    case BinaryOperator::IShl: return "ishl";
    case BinaryOperator::IShr: return "ishr";
    case BinaryOperator::ICmpEQ: return "icmpeq";
    case BinaryOperator::ICmpNE: return "icmpne";
    case BinaryOperator::ICmpLE: return "icmple";
    case BinaryOperator::ICmpGE: return "icmpge";
    case BinaryOperator::ICmpLT: return "icmplt";
    case BinaryOperator::ICmpGT: return "icmpgt";
    // boolean
    case BinaryOperator::BAnd: return "band";
    case BinaryOperator::BOr: return "bor";
    case BinaryOperator::BXor: return "bxor";
    // string
    case BinaryOperator::SAdd: return "sadd";
    case BinaryOperator::SSubStr: return "ssubstr";
    case BinaryOperator::SCmpEQ: return "scmpeq";
    case BinaryOperator::SCmpNE: return "scmpne";
    case BinaryOperator::SCmpLE: return "scmple";
    case BinaryOperator::SCmpGE: return "scmpge";
    case BinaryOperator::SCmpLT: return "scmplt";
    case BinaryOperator::SCmpGT: return "scmpgt";
    case BinaryOperator::SCmpRE: return "scmpre";
    case BinaryOperator::SCmpBeg: return "scmpbeg";
    case BinaryOperator::SCmpEnd: return "scmpend";
    case BinaryOperator::SIn: return "sin";
    // ip
    case BinaryOperator::PCmpEQ: return "pcmpeq";
    case BinaryOperator::PCmpNE: return "pcmpne";
    case BinaryOperator::PInCidr: return "pincidr";
    default: return "?";
  };
}
// }}}
// {{{ NopInstr
void NopInstr::dump() { dumpOne("NOP"); }

std::unique_ptr<Instr> NopInstr::clone() {
  return std::make_unique<NopInstr>();
}

void NopInstr::accept(InstructionVisitor& v) { v.visit(*this); }
// }}}
// {{{ CastInstr
void CastInstr::dump() {
  dumpOne((std::string("cast ") + tos(type()).c_str()).c_str());
}

std::unique_ptr<Instr> CastInstr::clone() {
  return std::make_unique<CastInstr>(type(), source(), name());
}

void CastInstr::accept(InstructionVisitor& v) {
  v.visit(*this);
}
// }}}
// {{{ CondBrInstr
CondBrInstr::CondBrInstr(Value* cond, BasicBlock* trueBlock, BasicBlock* falseBlock)
    : TerminateInstr({cond, trueBlock, falseBlock}) {}

void CondBrInstr::dump() { dumpOne("condbr"); }

std::unique_ptr<Instr> CondBrInstr::clone() {
  return std::make_unique<CondBrInstr>(condition(), trueBlock(), falseBlock());
}

void CondBrInstr::accept(InstructionVisitor& visitor) { visitor.visit(*this); }
// }}}
// {{{ BrInstr
BrInstr::BrInstr(BasicBlock* targetBlock) : TerminateInstr({targetBlock}) {}

void BrInstr::dump() { dumpOne("br"); }

std::unique_ptr<Instr> BrInstr::clone() {
  return std::make_unique<BrInstr>(targetBlock());
}

void BrInstr::accept(InstructionVisitor& visitor) { visitor.visit(*this); }
// }}}
// {{{ MatchInstr
MatchInstr::MatchInstr(MatchClass op, Value* cond)
    : TerminateInstr({cond, nullptr}), op_(op) {}

void MatchInstr::addCase(Constant* label, BasicBlock* code) {
  addOperand(label);
  addOperand(code);
}

void MatchInstr::setElseBlock(BasicBlock* code) { setOperand(1, code); }

BasicBlock* MatchInstr::elseBlock() const {
  return static_cast<BasicBlock*>(operand(1));
}

void MatchInstr::dump() {
  switch (op()) {
    case MatchClass::Same:
      dumpOne("match.same");
      break;
    case MatchClass::Head:
      dumpOne("match.head");
      break;
    case MatchClass::Tail:
      dumpOne("match.tail");
      break;
    case MatchClass::RegExp:
      dumpOne("match.re");
      break;
  }
}

MatchInstr::MatchInstr(const MatchInstr& v) : TerminateInstr(v), op_(v.op()) {}

std::unique_ptr<Instr> MatchInstr::clone() {
  return std::make_unique<MatchInstr>(*this);
}

std::vector<std::pair<Constant*, BasicBlock*>> MatchInstr::cases() const {
  std::vector<std::pair<Constant*, BasicBlock*>> out;

  size_t caseCount = (operands().size() - 2) / 2;

  for (size_t i = 0; i < caseCount; ++i) {
    Constant* label = static_cast<Constant*>(operand(2 + 2 * i + 0));
    BasicBlock* code = static_cast<BasicBlock*>(operand(2 + 2 * i + 1));

    out.push_back(std::make_pair(label, code));
  }

  return out;
}

void MatchInstr::accept(InstructionVisitor& visitor) { visitor.visit(*this); }
// }}}
// {{{ RetInstr
RetInstr::RetInstr(Value* result) : TerminateInstr({result}) {}

void RetInstr::dump() { dumpOne("ret"); }

std::unique_ptr<Instr> RetInstr::clone() {
  return std::make_unique<RetInstr>(operand(0));
}

void RetInstr::accept(InstructionVisitor& visitor) { visitor.visit(*this); }
// }}}
// {{{ CallInstr
CallInstr::CallInstr(const std::vector<Value*>& args, const std::string& name)
    : Instr(static_cast<IRBuiltinFunction*>(args[0])->signature().returnType(),
            args, name) {}

CallInstr::CallInstr(IRBuiltinFunction* callee, const std::vector<Value*>& args,
                     const std::string& name)
    : Instr(callee->signature().returnType(), join(callee, args), name) {}

void CallInstr::dump() {
  dumpOne("call");
}

std::unique_ptr<Instr> CallInstr::clone() {
  return std::make_unique<CallInstr>(operands(), name());
}

void CallInstr::accept(InstructionVisitor& visitor) {
  visitor.visit(*this);
}
// }}}
// {{{ HandlerCallInstr
HandlerCallInstr::HandlerCallInstr(const std::vector<Value*>& args)
    : Instr(LiteralType::Void, args, "") {}

HandlerCallInstr::HandlerCallInstr(IRBuiltinHandler* callee,
                                   const std::vector<Value*>& args)
    : Instr(LiteralType::Void, join(callee, args), "") {
  // XXX a handler call actually returns a boolean, but that's never used except
  // by the execution engine.
}

void HandlerCallInstr::dump() { dumpOne("handler"); }

std::unique_ptr<Instr> HandlerCallInstr::clone() {
  return std::make_unique<HandlerCallInstr>(operands());
}

void HandlerCallInstr::accept(InstructionVisitor& visitor) {
  visitor.visit(*this);
}
// }}}
// {{{ PhiNode
PhiNode::PhiNode(const std::vector<Value*>& ops, const std::string& name)
    : Instr(ops[0]->type(), ops, name) {}

void PhiNode::dump() { dumpOne("phi"); }

std::unique_ptr<Instr> PhiNode::clone() {
  return std::make_unique<PhiNode>(operands(), name());
}

void PhiNode::accept(InstructionVisitor& visitor) { visitor.visit(*this); }
// }}}
// {{{ other instructions
void AllocaInstr::accept(InstructionVisitor& visitor) { visitor.visit(*this); }

void StoreInstr::accept(InstructionVisitor& visitor) { visitor.visit(*this); }

void LoadInstr::accept(InstructionVisitor& visitor) { visitor.visit(*this); }

void AllocaInstr::dump() { dumpOne("alloca"); }

std::unique_ptr<Instr> AllocaInstr::clone() {
  return std::make_unique<AllocaInstr>(type(), operand(0), name());
}

void LoadInstr::dump() { dumpOne("load"); }

std::unique_ptr<Instr> LoadInstr::clone() {
  return std::make_unique<LoadInstr>(variable(), name());
}

void StoreInstr::dump() { dumpOne("store"); }

std::unique_ptr<Instr> StoreInstr::clone() {
  return std::make_unique<StoreInstr>(variable(), index(), source(), name());
}
// }}}

}  // namespace xzero::flow
