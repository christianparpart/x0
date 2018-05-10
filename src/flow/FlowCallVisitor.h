// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <flow/ASTVisitor.h>
#include <vector>

namespace xzero::flow {

//! \addtogroup Flow
//@{

class ASTNode;

class FlowCallVisitor : public ASTVisitor {
 private:
  std::vector<CallExpr*> calls_;

 public:
  explicit FlowCallVisitor(ASTNode* root = nullptr);
  ~FlowCallVisitor();

  void visit(ASTNode* root);

  void clear() { calls_.clear(); }

  const std::vector<CallExpr*>& calls() const { return calls_; }

 protected:
  // symbols
  void accept(UnitSym& symbol) override;
  void accept(VariableSym& variable) override;
  void accept(HandlerSym& handler) override;
  void accept(BuiltinFunctionSym& symbol) override;
  void accept(BuiltinHandlerSym& symbol) override;

  // expressions
  void accept(UnaryExpr& expr) override;
  void accept(BinaryExpr& expr) override;
  void accept(CallExpr& expr) override;
  void accept(VariableExpr& expr) override;
  void accept(HandlerRefExpr& expr) override;

  void accept(StringExpr& expr) override;
  void accept(NumberExpr& expr) override;
  void accept(BoolExpr& expr) override;
  void accept(RegExpExpr& expr) override;
  void accept(IPAddressExpr& expr) override;
  void accept(CidrExpr& cidr) override;
  void accept(ArrayExpr& array) override;

  // statements
  void accept(ExprStmt& stmt) override;
  void accept(CompoundStmt& stmt) override;
  void accept(CondStmt& stmt) override;
  void accept(MatchStmt& stmt) override;
  void accept(AssignStmt& stmt) override;
};

//!@}

}  // namespace xzero::flow
