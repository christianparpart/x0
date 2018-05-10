// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ir/BasicBlock.h>
#include <flow/ir/ConstantValue.h>
#include <flow/ir/IRBuilder.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/Instructions.h>
#include <flow/util/strings.h>

#include <assert.h>
#include <inttypes.h>
#include <math.h>

namespace xzero::flow {

IRBuilder::IRBuilder()
    : program_(nullptr),
      handler_(nullptr),
      insertPoint_(nullptr),
      nameStore_() {}

IRBuilder::~IRBuilder() {
}

// {{{ name management
std::string IRBuilder::makeName(const std::string& name) {
  const std::string theName = name.empty() ? "tmp" : name;

  auto i = nameStore_.find(theName);
  if (i == nameStore_.end()) {
    nameStore_[theName] = 0;
    return theName;
  }

  unsigned long id = ++i->second;

  char buf[512];
  snprintf(buf, sizeof(buf), "%s%lu", theName.c_str(), id);
  return buf;
}
// }}}
// {{{ context management
void IRBuilder::setProgram(std::unique_ptr<IRProgram> prog) {
  program_ = prog.release();
  handler_ = nullptr;
  insertPoint_ = nullptr;
}

IRHandler* IRBuilder::setHandler(IRHandler* hn) {
  assert(hn->getProgram() == program_);

  handler_ = hn;
  insertPoint_ = nullptr;

  return hn;
}

BasicBlock* IRBuilder::createBlock(const std::string& name) {
  std::string n = makeName(name);
  return handler_->createBlock(n);
}

void IRBuilder::setInsertPoint(BasicBlock* bb) {
  assert(bb != nullptr);
  assert(bb->getHandler() == handler() &&
         "insert point must belong to the current handler.");

  insertPoint_ = bb;
}

Instr* IRBuilder::insert(std::unique_ptr<Instr> instr) {
  assert(getInsertPoint() != nullptr);

  return getInsertPoint()->push_back(std::move(instr));
}
// }}}
// {{{ handler pool
IRHandler* IRBuilder::getHandler(const std::string& name) {
  if (IRHandler* h = program_->findHandler(name); h != nullptr)
    return h;

  return program_->createHandler(name);
}
// }}}
// {{{ value management
/**
 * dynamically allocates an array of given element type and size.
 */
AllocaInstr* IRBuilder::createAlloca(LiteralType ty, Value* arraySize,
                                     const std::string& name) {
  return static_cast<AllocaInstr*>(
      insert<AllocaInstr>(ty, arraySize, makeName(name)));
}

/**
 * Loads given value
 */
Value* IRBuilder::createLoad(Value* value, const std::string& name) {
  if (dynamic_cast<Constant*>(value))
    return value;

  // if (dynamic_cast<Variable*>(value))
  return insert<LoadInstr>(value, makeName(name));

  assert(!"Value must be of type Constant or Variable.");
  return nullptr;
}

/**
 * emits a STORE of value \p rhs to variable \p lhs.
 */
Instr* IRBuilder::createStore(Value* lhs, Value* rhs, const std::string& name) {
  return createStore(lhs, get(0), rhs, name);
}

Instr* IRBuilder::createStore(Value* lhs, ConstantInt* index, Value* rhs,
                              const std::string& name) {
  assert(dynamic_cast<AllocaInstr*>(lhs) &&
         "lhs must be of type AllocaInstr in order to STORE to.");
  // assert(lhs->type() == rhs->type() && "Type of lhs and rhs must be equal.");
  // assert(dynamic_cast<IRVariable*>(lhs) && "lhs must be of type Variable.");

  return insert<StoreInstr>(lhs, index, rhs, makeName(name));
}

Instr* IRBuilder::createPhi(const std::vector<Value*>& incomings,
                            const std::string& name) {
  return insert<PhiNode>(incomings, makeName(name));
}
// }}}
// {{{ boolean ops
Value* IRBuilder::createBNot(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::Boolean);

  if (auto a = dynamic_cast<ConstantBoolean*>(rhs))
    return getBoolean(!a->get());

  return insert<BNotInstr>(rhs, makeName(name));
}

Value* IRBuilder::createBAnd(Value* lhs, Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::Boolean);

  if (auto a = dynamic_cast<ConstantBoolean*>(lhs))
    if (auto b = dynamic_cast<ConstantBoolean*>(rhs))
      return getBoolean(a->get() && b->get());

  return insert<BAndInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createBXor(Value* lhs, Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::Boolean);

  if (auto a = dynamic_cast<ConstantBoolean*>(lhs))
    if (auto b = dynamic_cast<ConstantBoolean*>(rhs))
      return getBoolean(a->get() ^ b->get());

  return insert<BAndInstr>(lhs, rhs, makeName(name));
}
// }}}
// {{{ numerical ops
Value* IRBuilder::createNeg(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(rhs))
    return get(-a->get());

  return insert<INegInstr>(rhs, makeName(name));
}

Value* IRBuilder::createNot(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(rhs))
    return get(~a->get());

  return insert<INotInstr>(rhs, makeName(name));
}

Value* IRBuilder::createAdd(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() + b->get());

  return insert<IAddInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSub(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() - b->get());

  return insert<ISubInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createMul(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() * b->get());

  return insert<IMulInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createDiv(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() / b->get());

  return insert<IDivInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createRem(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() % b->get());

  return insert<IRemInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createShl(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() << b->get());

  return insert<IShlInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createShr(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() >> b->get());

  return insert<IShrInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createPow(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(powl(a->get(), b->get()));

  return insert<IPowInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createAnd(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() & b->get());

  return insert<IAndInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createOr(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() | b->get());

  return insert<IOrInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createXor(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return get(a->get() ^ b->get());

  return insert<IXorInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createNCmpEQ(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return getBoolean(a->get() == b->get());

  return insert<ICmpEQInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createNCmpNE(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return getBoolean(a->get() != b->get());

  return insert<ICmpNEInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createNCmpLE(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return getBoolean(a->get() <= b->get());

  return insert<ICmpLEInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createNCmpGE(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return getBoolean(a->get() >= b->get());

  return insert<ICmpGEInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createNCmpLT(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return getBoolean(a->get() < b->get());

  return insert<ICmpLTInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createNCmpGT(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::Number);

  if (auto a = dynamic_cast<ConstantInt*>(lhs))
    if (auto b = dynamic_cast<ConstantInt*>(rhs))
      return getBoolean(a->get() > b->get());

  return insert<ICmpGTInstr>(lhs, rhs, makeName(name));
}
// }}}
// {{{ string ops
Value* IRBuilder::createSAdd(Value* lhs, Value* rhs, const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::String);

  if (auto a = dynamic_cast<ConstantString*>(lhs)) {
    if (auto b = dynamic_cast<ConstantString*>(rhs)) {
      return get(a->get() + b->get());
    }

    if (a->get().empty()) {
      return rhs;
    }
  } else if (auto b = dynamic_cast<ConstantString*>(rhs)) {
    if (b->get().empty()) {
      return rhs;
    }
  }

  return insert<SAddInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSCmpEQ(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::String);

  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(a->get() == b->get());

  return insert<SCmpEQInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSCmpNE(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::String);

  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(a->get() != b->get());

  return insert<SCmpNEInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSCmpLE(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::String);

  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(a->get() <= b->get());

  return insert<SCmpLEInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSCmpGE(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::String);

  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(a->get() >= b->get());

  return insert<SCmpGEInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSCmpLT(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::String);

  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(a->get() < b->get());

  return insert<SCmpLTInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSCmpGT(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == rhs->type());
  assert(lhs->type() == LiteralType::String);

  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(a->get() > b->get());

  return insert<SCmpGTInstr>(lhs, rhs, makeName(name));
}

/**
 * Compare string \p lhs against regexp \p rhs.
 */
Value* IRBuilder::createSCmpRE(Value* lhs, Value* rhs,
                               const std::string& name) {
  assert(lhs->type() == LiteralType::String);
  assert(rhs->type() == LiteralType::RegExp);

  // XXX don't perform constant folding on (string =~ regexp) as this operation
  // yields side affects to: regex.group(I)S

  return insert<SCmpREInstr>(lhs, rhs, makeName(name));
}

/**
 * Tests if string \p lhs begins with string \p rhs.
 *
 * @param lhs test string
 * @param rhs sub string needle
 * @param name Name of the given operations result value.
 *
 * @return boolean result.
 */
Value* IRBuilder::createSCmpEB(Value* lhs, Value* rhs,
                               const std::string& name) {
  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(beginsWith(a->get(), b->get()));

  return insert<SCmpBegInstr>(lhs, rhs, makeName(name));
}

/**
 * Tests if string \p lhs ends with string \p rhs.
 *
 * @param lhs test string
 * @param rhs sub string needle
 * @param name Name of the given operations result value.
 *
 * @return boolean result.
 */
Value* IRBuilder::createSCmpEE(Value* lhs, Value* rhs,
                               const std::string& name) {
  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(endsWith(a->get(), b->get()));

  return insert<SCmpEndInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSIn(Value* lhs, Value* rhs, const std::string& name) {
  if (auto a = dynamic_cast<ConstantString*>(lhs))
    if (auto b = dynamic_cast<ConstantString*>(rhs))
      return getBoolean(b->get().find(a->get()) != std::string::npos);

  return insert<SInInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createSLen(Value* value, const std::string& name) {
  if (auto a = dynamic_cast<ConstantString*>(value))
    return get(a->get().size());

  return insert<SLenInstr>(value, makeName(name));
}
// }}}
// {{{ ip ops
Value* IRBuilder::createPCmpEQ(Value* lhs, Value* rhs,
                               const std::string& name) {
  if (auto a = dynamic_cast<ConstantIP*>(lhs))
    if (auto b = dynamic_cast<ConstantIP*>(rhs))
      return getBoolean(a->get() == b->get());

  return insert<PCmpEQInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createPCmpNE(Value* lhs, Value* rhs,
                               const std::string& name) {
  if (auto a = dynamic_cast<ConstantIP*>(lhs))
    if (auto b = dynamic_cast<ConstantIP*>(rhs))
      return getBoolean(a->get() != b->get());

  return insert<PCmpNEInstr>(lhs, rhs, makeName(name));
}

Value* IRBuilder::createPInCidr(Value* lhs, Value* rhs,
                                const std::string& name) {
  if (auto a = dynamic_cast<ConstantIP*>(lhs))
    if (auto b = dynamic_cast<ConstantCidr*>(rhs))
      return getBoolean(b->get().contains(a->get()));

  return insert<PInCidrInstr>(lhs, rhs, makeName(name));
}
// }}}
// {{{ cast ops
Value* IRBuilder::createB2S(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::Boolean);

  if (auto a = dynamic_cast<ConstantBoolean*>(rhs))
    return get(a->get() ? "true" : "false");

  return insert<CastInstr>(LiteralType::String, rhs, makeName(name));
}

Value* IRBuilder::createN2S(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::Number);

  if (auto i = dynamic_cast<ConstantInt*>(rhs)) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%" PRIi64 "", i->get());
    return get(buf);
  }

  return insert<CastInstr>(LiteralType::String, rhs, makeName(name));
}

Value* IRBuilder::createP2S(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::IPAddress);

  if (auto ip = dynamic_cast<ConstantIP*>(rhs)) return get(ip->get().str());

  return insert<CastInstr>(LiteralType::String, rhs, makeName(name));
}

Value* IRBuilder::createC2S(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::Cidr);

  if (auto ip = dynamic_cast<ConstantCidr*>(rhs)) return get(ip->get().str());

  return insert<CastInstr>(LiteralType::String, rhs, makeName(name));
}

Value* IRBuilder::createR2S(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::RegExp);

  if (auto ip = dynamic_cast<ConstantRegExp*>(rhs))
    return get(ip->get().pattern());

  return insert<CastInstr>(LiteralType::String, rhs, makeName(name));
}

Value* IRBuilder::createS2N(Value* rhs, const std::string& name) {
  assert(rhs->type() == LiteralType::String);

  if (auto value = dynamic_cast<ConstantString*>(rhs)) {
    try {
      return get(stoi(value->get()));
    } catch (...) {
      // fall through to default behaviour
    }
  }

  return insert<CastInstr>(LiteralType::Number, rhs, makeName(name));
}
// }}}
// {{{ call creators
Instr* IRBuilder::createCallFunction(IRBuiltinFunction* callee,
                                     const std::vector<Value*>& args,
                                     const std::string& name) {
  return insert<CallInstr>(callee, args, makeName(name));
}

Instr* IRBuilder::createInvokeHandler(IRBuiltinHandler* callee,
                                      const std::vector<Value*>& args) {
  return insert<HandlerCallInstr>(callee, args);
}
// }}}
// {{{ exit point creators
Instr* IRBuilder::createRet(Value* result) {
  return insert<RetInstr>(result);
}

Instr* IRBuilder::createBr(BasicBlock* target) {
  return insert<BrInstr>(target);
}

Instr* IRBuilder::createCondBr(Value* condValue, BasicBlock* trueBlock,
                               BasicBlock* falseBlock) {
  return insert<CondBrInstr>(condValue, trueBlock, falseBlock);
}

MatchInstr* IRBuilder::createMatch(MatchClass opc, Value* cond) {
  return static_cast<MatchInstr*>(insert<MatchInstr>(opc, cond));
}

Value* IRBuilder::createMatchSame(Value* cond) {
  return createMatch(MatchClass::Same, cond);
}

Value* IRBuilder::createMatchHead(Value* cond) {
  return createMatch(MatchClass::Head, cond);
}

Value* IRBuilder::createMatchTail(Value* cond) {
  return createMatch(MatchClass::Tail, cond);
}

Value* IRBuilder::createMatchRegExp(Value* cond) {
  return createMatch(MatchClass::RegExp, cond);
}
// }}}

}  // namespace xzero::flow
