// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#include <flow/ASTVisitor.h>

namespace xzero {
namespace flow {

//! \addtogroup Flow
//@{

class ASTNode;

class ASTPrinter : public ASTVisitor {
 public:
  static void print(ASTNode* node);

 private:
  ASTPrinter();

  int depth_;

  void enter() { ++depth_; }
  void leave() { --depth_; }
  void prefix();
  void print(const char* title, ASTNode* node);
  void print(const std::pair<std::string, Expr*>& node, size_t pos);

  void printf(const char* msg) {
    prefix();
    std::printf("%s", msg);
  }
  template <typename... Args>
  void printf(const char* fmt, Args... args) {
    prefix();
    std::printf(fmt, args...);
  }

  void accept(VariableSym& variable) override;
  void accept(HandlerSym& handler) override;
  void accept(BuiltinFunctionSym& symbol) override;
  void accept(BuiltinHandlerSym& symbol) override;
  void accept(UnitSym& symbol) override;

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
  void accept(ExprStmt& stmt) override;

  void accept(CompoundStmt& stmt) override;
  void accept(CondStmt& stmt) override;
  void accept(MatchStmt& stmt) override;
  void accept(AssignStmt& stmt) override;
};

//!@}

}  // namespace flow
}  // namespace xzero
