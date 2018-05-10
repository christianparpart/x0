// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/IRGenerator.h>
#include <flow/NativeCallback.h>
#include <flow/AST.h>
#include <flow/ir/IRProgram.h>
#include <flow/ir/IRHandler.h>
#include <flow/ir/Instructions.h>
#include <flow/ir/ConstantArray.h>
#include <algorithm>
#include <assert.h>
#include <math.h>

namespace xzero {
namespace flow {

IRGenerator::IRGenerator()
    : IRGenerator{ErrorHandler{}, {}} {}

IRGenerator::IRGenerator(ErrorHandler eh, std::vector<std::string> exports)
    : IRBuilder(),
      ASTVisitor(),
      exports_(std::move(exports)),
      scope_(std::make_unique<Scope>()),
      result_(nullptr),
      handlerStack_(),
      errorCount_(0),
      onError_(std::move(eh)) {}

std::unique_ptr<IRProgram> IRGenerator::generate(
    UnitSym* unit, const std::vector<std::string>& exportedHandlers) {
  return IRGenerator{ErrorHandler{}, exportedHandlers}.generate(unit);
}

std::unique_ptr<IRProgram> IRGenerator::generate(UnitSym* unit) {
  codegen(unit);

  if (errorCount_ > 0)
    return nullptr;

  return std::unique_ptr<IRProgram>(program());
}

Value* IRGenerator::codegen(Expr* expr) {
  expr->visit(*this);
  return result_;
}

Value* IRGenerator::codegen(Stmt* stmt) {
  if (stmt) {
    stmt->visit(*this);
  } else {
    result_ = nullptr;
  }
  return result_;
}

Value* IRGenerator::codegen(Symbol* sym) {
  sym->visit(*this);
  return result_;
}

void IRGenerator::accept(UnitSym& unit) {
  setProgram(std::make_unique<IRProgram>());
  program()->setModules(unit.modules());

  for (const std::unique_ptr<Symbol>& sym : *unit.scope()) {
    codegen(sym.get());
  }
}

// # i = 2 + 3 * 4
// IPUSH 2
// IPUSH 3
// IPUSH 4
// NMUL
// NADD
//
// # printnum i
// PUSH STACK[0]
// CALL @printnum
//
// # i = i + 2
// PUSH STACK[0]
// PUSH 2
// NADD
// NPOP STACK[0]

void IRGenerator::accept(VariableSym& variable) {
  AllocaInstr* var = createAlloca(variable.initializer()->getType(),
                                  get(1),
                                  variable.name());
  scope().update(&variable, var);

  Value* initializer = codegen(variable.initializer());
  assert(initializer != nullptr);

  createStore(var, initializer);

  result_ = var;
}

void IRGenerator::accept(HandlerSym& handlerSym) {
  assert(handlerStack_.empty());

  if (!exports_.empty()) {
    auto i = std::find(exports_.begin(), exports_.end(), handlerSym.name());
    if (i == exports_.end()) {
      return;
    }
  }

  setHandler(getHandler(handlerSym.name()));
  setInsertPoint(createBlock("EntryPoint"));

  codegenInline(handlerSym);

  createRet(get(false));

  handler()->verify();

  assert(handlerStack_.empty());
}

void IRGenerator::codegenInline(HandlerSym& handlerSym) {
  auto i = std::find(handlerStack_.begin(), handlerStack_.end(), &handlerSym);
  if (i != handlerStack_.end()) {
    reportError("Cannot recursively call handler %s.",
                handlerSym.name().c_str());

    return;
  }

  handlerStack_.push_back(&handlerSym);

  // emit local variable declarations
  if (handlerSym.scope()) {
    for (std::unique_ptr<Symbol>& symbol : *handlerSym.scope()) {
      codegen(symbol.get());
    }
  }

  if (handlerSym.body() == nullptr) {
    reportError("Forward declared handler '%s' is missing implementation.",
                handlerSym.name().c_str());
  }

  // emit body
  codegen(handlerSym.body());

  handlerStack_.pop_back();
}

void IRGenerator::accept(BuiltinFunctionSym& builtin) {
  result_ = getBuiltinFunction(*builtin.nativeCallback());
}

void IRGenerator::accept(BuiltinHandlerSym& builtin) {
  result_ = getBuiltinHandler(*builtin.nativeCallback());
}

void IRGenerator::accept(UnaryExpr& expr) {
  static const std::unordered_map<
      int /*Opcode*/,
      Value* (IRGenerator::*)(Value*, const std::string&)> ops =
      {{Opcode::N2S, &IRGenerator::createN2S},
       {Opcode::P2S, &IRGenerator::createP2S},
       {Opcode::C2S, &IRGenerator::createC2S},
       {Opcode::R2S, &IRGenerator::createR2S},
       {Opcode::S2N, &IRGenerator::createS2N},
       {Opcode::NNEG, &IRGenerator::createNeg},
       {Opcode::NNOT, &IRGenerator::createNot},
       {Opcode::BNOT, &IRGenerator::createBNot},
       {Opcode::SLEN, &IRGenerator::createSLen}, };

  Value* rhs = codegen(expr.subExpr());

  auto i = ops.find(expr.op());
  FLOW_ASSERT(i != ops.end(), fmt::format("Unsupported unary expression {} in IRGenerator.", mnemonic(expr.op())));
  result_ = (this->*i->second)(rhs, "");
}

void IRGenerator::accept(BinaryExpr& expr) {
  static const std::unordered_map<
      int /*Opcode*/,
      Value* (IRGenerator::*)(Value*, Value*, const std::string&)> ops =
      {// boolean
       {Opcode::BAND, &IRGenerator::createBAnd},
       {Opcode::BXOR, &IRGenerator::createBXor},
       // numerical
       {Opcode::NADD, &IRGenerator::createAdd},
       {Opcode::NSUB, &IRGenerator::createSub},
       {Opcode::NMUL, &IRGenerator::createMul},
       {Opcode::NDIV, &IRGenerator::createDiv},
       {Opcode::NREM, &IRGenerator::createRem},
       {Opcode::NSHL, &IRGenerator::createShl},
       {Opcode::NSHR, &IRGenerator::createShr},
       {Opcode::NPOW, &IRGenerator::createPow},
       {Opcode::NAND, &IRGenerator::createAnd},
       {Opcode::NOR, &IRGenerator::createOr},
       {Opcode::NXOR, &IRGenerator::createXor},
       {Opcode::NCMPEQ, &IRGenerator::createNCmpEQ},
       {Opcode::NCMPNE, &IRGenerator::createNCmpNE},
       {Opcode::NCMPLE, &IRGenerator::createNCmpLE},
       {Opcode::NCMPGE, &IRGenerator::createNCmpGE},
       {Opcode::NCMPLT, &IRGenerator::createNCmpLT},
       {Opcode::NCMPGT, &IRGenerator::createNCmpGT},

       // string
       {Opcode::SADD, &IRGenerator::createSAdd},
       {Opcode::SCMPEQ, &IRGenerator::createSCmpEQ},
       {Opcode::SCMPNE, &IRGenerator::createSCmpNE},
       {Opcode::SCMPLE, &IRGenerator::createSCmpLE},
       {Opcode::SCMPGE, &IRGenerator::createSCmpGE},
       {Opcode::SCMPLT, &IRGenerator::createSCmpLT},
       {Opcode::SCMPGT, &IRGenerator::createSCmpGT},
       {Opcode::SCMPBEG, &IRGenerator::createSCmpEB},
       {Opcode::SCMPEND, &IRGenerator::createSCmpEE},
       {Opcode::SCONTAINS, &IRGenerator::createSIn},

       // regex
       {Opcode::SREGMATCH, &IRGenerator::createSCmpRE},

       // ip
       {Opcode::PCMPEQ, &IRGenerator::createPCmpEQ},
       {Opcode::PCMPNE, &IRGenerator::createPCmpNE},
       {Opcode::PINCIDR, &IRGenerator::createPInCidr}, };

  if (expr.op() == Opcode::BOR) {
    // (lhs || rhs)
    //
    //   L = lhs();
    //   if (L) goto end;
    //   R = rhs();
    //   L = R;
    // end:
    //   result = L;

    BasicBlock* borLeft = createBlock("bor.left");
    BasicBlock* borRight = createBlock("bor.right");
    BasicBlock* borCont = createBlock("bor.cont");

    AllocaInstr* result = createAlloca(LiteralType::Boolean, get(1), "bor");
    Value* lhs = codegen(expr.leftExpr());
    createCondBr(lhs, borLeft, borRight);

    setInsertPoint(borLeft);
    createStore(result, lhs, "bor.left");
    createBr(borCont);

    setInsertPoint(borRight);
    Value* rhs = codegen(expr.rightExpr());
    createStore(result, rhs, "bor.right");
    createBr(borCont);

    setInsertPoint(borCont);

    result_ = result;

    return;
  }

  Value* lhs = codegen(expr.leftExpr());
  Value* rhs = codegen(expr.rightExpr());

  auto i = ops.find(expr.op());
  if (i != ops.end()) {
    result_ = (this->*i->second)(lhs, rhs, "");
  } else {
    fprintf(stderr, "BUG: Binary operation `%s` not implemented.\n",
            mnemonic(expr.op()));
    assert(!"Unimplemented");
    result_ = nullptr;
  }
}

void IRGenerator::accept(CallExpr& call) {
  std::vector<Value*> args;
  for (const std::unique_ptr<Expr>& arg : call.args().values()) {
    if (Value* v = codegen(arg.get())) {
      args.push_back(v);
    } else {
      return;
    }
  }

  if (call.callee()->isFunction()) {
    Value* callee = codegen(call.callee());
    // builtin function
    result_ = createCallFunction(static_cast<IRBuiltinFunction*>(callee), args);
  } else if (call.callee()->isBuiltin()) {
    Value* callee = codegen(call.callee());
    // builtin handler
    result_ = createInvokeHandler(static_cast<IRBuiltinHandler*>(callee), args);
  } else {
    // source handler
    codegenInline(*static_cast<HandlerSym*>(call.callee()));
    result_ = nullptr;
  }
}

void IRGenerator::accept(VariableExpr& expr) {
  // loads the value of the given variable

  if (auto var = scope().lookup(expr.variable())) {
    result_ = createLoad(var);
  } else {
    result_ = nullptr;
  }
}

void IRGenerator::accept(HandlerRefExpr& literal) {
  // lodas a handler reference (handler ID) to a handler, possibly generating
  // the code for this handler.

  result_ = codegen(literal.handler());
}

void IRGenerator::accept(StringExpr& literal) {
  // loads a string literal

  result_ = get(literal.value());
}

void IRGenerator::accept(NumberExpr& literal) {
  // loads a number literal

  result_ = get(literal.value());
}

void IRGenerator::accept(BoolExpr& literal) {
  // loads a boolean literal

  result_ = getBoolean(literal.value());
}

void IRGenerator::accept(RegExpExpr& literal) {
  // loads a regex literal by reference ID to the const table

  result_ = get(literal.value());
}

void IRGenerator::accept(IPAddressExpr& literal) {
  // loads an ip address by reference ID to the const table

  result_ = get(literal.value());
}

void IRGenerator::accept(CidrExpr& literal) {
  // loads a CIDR network by reference ID to the const table

  result_ = get(literal.value());
}

bool isConstant(const std::vector<Value*>& values) {
  for (const Value* value : values) {
    if (!dynamic_cast<const Constant*>(value)) {
      return false;
    }
  }
  return true;
}

void IRGenerator::accept(ArrayExpr& arrayExpr) {
  std::vector<Value*> values;
  for (size_t i = 0, e = arrayExpr.values().size(); i != e; ++i) {
    Value* element = codegen(arrayExpr.values()[i].get());
    values.push_back(element);
  }

  if (isConstant(values)) {
    std::vector<Constant*> constants;
    for (Value* value : values)
      constants.push_back(static_cast<Constant*>(value));

    result_ = get(constants);
  } else {
    // TODO: print line:col hint where this exact message occured.
    // via: reportError(arrayExpr, "VariableSym array elements not allowed.");
    reportError("VariableSym array elements not allowed.");
    result_ = nullptr;
  }
}

void IRGenerator::accept(ExprStmt& exprStmt) {
  codegen(exprStmt.expression());
}

void IRGenerator::accept(CompoundStmt& compoundStmt) {
  for (const auto& stmt : compoundStmt) {
    codegen(stmt.get());
  }
}

void IRGenerator::accept(CondStmt& condStmt) {
  BasicBlock* trueBlock = createBlock("trueBlock");
  BasicBlock* falseBlock = createBlock("falseBlock");
  BasicBlock* contBlock = createBlock("contBlock");

  Value* cond = codegen(condStmt.condition());
  createCondBr(cond, trueBlock, falseBlock);

  setInsertPoint(trueBlock);
  codegen(condStmt.thenStmt());
  createBr(contBlock);

  setInsertPoint(falseBlock);
  codegen(condStmt.elseStmt());
  createBr(contBlock);

  setInsertPoint(contBlock);
}

Constant* IRGenerator::getConstant(Expr* expr) {
  if (auto e = dynamic_cast<StringExpr*>(expr))
    return get(e->value());
  else if (auto e = dynamic_cast<RegExpExpr*>(expr))
    return get(e->value());
  else {
    reportError("FIXME: Invalid (unsupported) literal type <%s> in match case.",
                tos(expr->getType()).c_str());
    return nullptr;
  }
}

void IRGenerator::accept(MatchStmt& matchStmt) {
  Value* cond = codegen(matchStmt.condition());
  BasicBlock* contBlock = createBlock("match.cont");
  MatchInstr* matchInstr = createMatch(matchStmt.op(), cond);

  for (const MatchStmt::Case& one : matchStmt.cases()) {
    BasicBlock* bb = createBlock("match.case");
    setInsertPoint(bb);
    codegen(one.second.get());
    createBr(contBlock);

    for (auto& labelNode : one.first) {
      Constant* label = getConstant(labelNode.get());
      matchInstr->addCase(label, bb);
    }
  }

  if (matchStmt.elseStmt()) {
    BasicBlock* elseBlock = createBlock("match.else");
    setInsertPoint(elseBlock);
    codegen(matchStmt.elseStmt());
    createBr(contBlock);

    matchInstr->setElseBlock(elseBlock);
  } else {
    matchInstr->setElseBlock(contBlock);
  }

  setInsertPoint(contBlock);
}

void IRGenerator::accept(AssignStmt& assignStmt) {
  Value* lhs = scope().lookup(assignStmt.variable());
  Value* rhs = codegen(assignStmt.expression());
  assert(lhs->type() == rhs->type() && "Type of lhs and rhs must be equal.");

  result_ = createStore(lhs, rhs, "assignment");
}

void IRGenerator::reportError(const std::string& message) {
  ++errorCount_;

  if (onError_) {
    onError_(message);
  } else {
    // default to print to stderr instead
    fprintf(stderr, "%s\n", message.c_str());
  }
}

}  // namespace flow
}  // namespace xzero
