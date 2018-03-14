// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT
#pragma once

#include <xzero-flow/ASTVisitor.h>
#include <xzero/defines.h>
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
  virtual void accept(UnitSym& symbol);
  virtual void accept(VariableSym& variable);
  virtual void accept(HandlerSym& handler);
  virtual void accept(BuiltinFunctionSym& symbol);
  virtual void accept(BuiltinHandlerSym& symbol);

  // expressions
  virtual void accept(UnaryExpr& expr);
  virtual void accept(BinaryExpr& expr);
  virtual void accept(CallExpr& expr);
  virtual void accept(VariableExpr& expr);
  virtual void accept(HandlerRefExpr& expr);

  virtual void accept(StringExpr& expr);
  virtual void accept(NumberExpr& expr);
  virtual void accept(BoolExpr& expr);
  virtual void accept(RegExpExpr& expr);
  virtual void accept(IPAddressExpr& expr);
  virtual void accept(CidrExpr& cidr);
  virtual void accept(ArrayExpr& array);

  // statements
  virtual void accept(ExprStmt& stmt);
  virtual void accept(CompoundStmt& stmt);
  virtual void accept(CondStmt& stmt);
  virtual void accept(MatchStmt& stmt);
  virtual void accept(ForStmt& stmt);
  virtual void accept(AssignStmt& stmt);
};

//!@}

}  // namespace xzero::flow
