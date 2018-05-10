// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/FlowCallVisitor.h>
#include <flow/AST.h>
#include <algorithm>
#include <cassert>

namespace xzero {
namespace flow {

FlowCallVisitor::FlowCallVisitor(ASTNode* root) : calls_() { visit(root); }

FlowCallVisitor::~FlowCallVisitor() {}

void FlowCallVisitor::visit(ASTNode* node) {
  if (node) {
    node->visit(*this);
  }
}

// {{{ symbols
void FlowCallVisitor::accept(VariableSym& variable) {
  visit(variable.initializer());
}

void FlowCallVisitor::accept(HandlerSym& handler) {
  if (handler.scope()) {
    for (std::unique_ptr<Symbol>& sym : *handler.scope()) {
      visit(sym.get());
    }
  }

  visit(handler.body());
}

void FlowCallVisitor::accept(BuiltinFunctionSym& v) {}

void FlowCallVisitor::accept(BuiltinHandlerSym& v) {}

void FlowCallVisitor::accept(UnitSym& unit) {
  for (std::unique_ptr<Symbol>& s : *unit.scope()) {
    visit(s.get());
  }
}
// }}}
// {{{ expressions
void FlowCallVisitor::accept(UnaryExpr& expr) { visit(expr.subExpr()); }

void FlowCallVisitor::accept(BinaryExpr& expr) {
  visit(expr.leftExpr());
  visit(expr.rightExpr());
}

void FlowCallVisitor::accept(CallExpr& call) {
  for (const auto& arg : call.args().values()) {
    visit(arg.get());
  }

  if (call.callee() && call.callee()->isBuiltin()) {
    calls_.push_back(&call);
  }
}

void FlowCallVisitor::accept(VariableExpr& expr) {}

void FlowCallVisitor::accept(HandlerRefExpr& expr) {}

void FlowCallVisitor::accept(StringExpr& expr) {}

void FlowCallVisitor::accept(NumberExpr& expr) {}

void FlowCallVisitor::accept(BoolExpr& expr) {}

void FlowCallVisitor::accept(RegExpExpr& expr) {}

void FlowCallVisitor::accept(IPAddressExpr& expr) {}

void FlowCallVisitor::accept(CidrExpr& expr) {}

void FlowCallVisitor::accept(ArrayExpr& array) {
  for (const auto& e : array.values()) {
    visit(e.get());
  }
}

void FlowCallVisitor::accept(ExprStmt& stmt) { visit(stmt.expression()); }
// }}}
// {{{ stmt
void FlowCallVisitor::accept(CompoundStmt& compound) {
  for (const auto& stmt : compound) {
    visit(stmt.get());
  }
}

void FlowCallVisitor::accept(CondStmt& condStmt) {
  visit(condStmt.condition());
  visit(condStmt.thenStmt());
  visit(condStmt.elseStmt());
}

void FlowCallVisitor::accept(MatchStmt& stmt) {
  visit(stmt.condition());
  for (auto& one : stmt.cases()) {
    for (auto& label : one.first) visit(label.get());

    visit(one.second.get());
  }
  visit(stmt.elseStmt());
}

void FlowCallVisitor::accept(AssignStmt& assignStmt) {
  visit(assignStmt.expression());
}
// }}}

}  // namespace flow
}  // namespace xzero
