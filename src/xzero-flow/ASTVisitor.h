// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero-flow/Api.h>
#include <xzero/RegExp.h>
#include <xzero/net/IPAddress.h>
#include <xzero/net/Cidr.h>
#include <utility>
#include <memory>

namespace xzero {
namespace flow {

//! \addtogroup Flow
//@{

class Symbol;
class SymbolTable;
class ScopedSymbol;
class Variable;
class Handler;
class BuiltinFunction;
class BuiltinHandler;
class Unit;

class Expr;
class BinaryExpr;
class UnaryExpr;
template <typename>
class LiteralExpr;
class CallExpr;
class VariableExpr;
class HandlerRefExpr;
class ArrayExpr;

typedef LiteralExpr<std::string> StringExpr;
typedef LiteralExpr<long long> NumberExpr;
typedef LiteralExpr<bool> BoolExpr;
typedef LiteralExpr<RegExp> RegExpExpr;
typedef LiteralExpr<IPAddress> IPAddressExpr;
typedef LiteralExpr<Cidr> CidrExpr;

class Stmt;
class ExprStmt;
class CompoundStmt;
class CondStmt;
class MatchStmt;
class ForStmt;
class AssignStmt;

class XZERO_FLOW_API ASTVisitor {
 public:
  virtual ~ASTVisitor() {}

  // symbols
  virtual void accept(Unit& symbol) = 0;
  virtual void accept(Variable& variable) = 0;
  virtual void accept(Handler& handler) = 0;
  virtual void accept(BuiltinFunction& symbol) = 0;
  virtual void accept(BuiltinHandler& symbol) = 0;

  // expressions
  virtual void accept(UnaryExpr& expr) = 0;
  virtual void accept(BinaryExpr& expr) = 0;
  virtual void accept(CallExpr& expr) = 0;
  virtual void accept(VariableExpr& expr) = 0;
  virtual void accept(HandlerRefExpr& expr) = 0;

  virtual void accept(StringExpr& expr) = 0;
  virtual void accept(NumberExpr& expr) = 0;
  virtual void accept(BoolExpr& expr) = 0;
  virtual void accept(RegExpExpr& expr) = 0;
  virtual void accept(IPAddressExpr& expr) = 0;
  virtual void accept(CidrExpr& array) = 0;
  virtual void accept(ArrayExpr& cidr) = 0;

  // statements
  virtual void accept(ExprStmt& stmt) = 0;
  virtual void accept(CompoundStmt& stmt) = 0;
  virtual void accept(CondStmt& stmt) = 0;
  virtual void accept(MatchStmt& stmt) = 0;
  virtual void accept(ForStmt& stmt) = 0;
  virtual void accept(AssignStmt& stmt) = 0;
};

//!@}

}  // namespace flow
}  // namespace xzero
