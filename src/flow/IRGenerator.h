// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2018 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <flow/ASTVisitor.h>
#include <flow/ir/IRBuilder.h>
#include <deque>
#include <vector>
#include <string>

namespace xzero {
namespace flow {

//! \addtogroup Flow
//@{

/**
 * Transforms a Flow-AST into an SSA-conform IR.
 *
 */
class IRGenerator : public IRBuilder, public ASTVisitor {
 public:
  using ErrorHandler = std::function<void(const std::string&)>;

  IRGenerator();
  IRGenerator(ErrorHandler eh, std::vector<std::string> exports);

  static std::unique_ptr<IRProgram> generate(
      UnitSym* unit, const std::vector<std::string>& exportedHandlers);

  std::unique_ptr<IRProgram> generate(UnitSym* unit);

 private:
  class Scope;

  std::vector<std::string> exports_;
  std::unique_ptr<Scope> scope_;
  Value* result_;
  std::deque<HandlerSym*> handlerStack_;

  size_t errorCount_;
  std::function<void(const std::string&)> onError_;

 private:
  Value* codegen(Expr* expr);
  Value* codegen(Stmt* stmt);
  Value* codegen(Symbol* sym);
  void codegenInline(HandlerSym& handlerSym);

  Constant* getConstant(Expr* expr);

  Scope& scope() { return *scope_; }

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

  // error handling
  void reportError(const std::string& message);
  template <typename... Args>
  void reportError(const std::string& fmt, Args&&...);
};

// {{{ class IRGenerator::Scope
class IRGenerator::Scope {
 private:
  std::unordered_map<Symbol*, Value*> scope_;

  std::unordered_map<Symbol*, std::list<Value*>> versions_; // TODO: unused?

 public:
  void clear() {
    scope_.clear();
  }

  Value* lookup(Symbol* symbol) const {
    if (auto i = scope_.find(symbol); i != scope_.end())
      return i->second;

    return nullptr;
  }

  void update(Symbol* symbol, Value* value) {
    scope_[symbol] = value;
  }

  void remove(Symbol* symbol) {
    auto i = scope_.find(symbol);
    if (i != scope_.end()) {
      scope_.erase(i);
    }
  }
};
// }}}

// {{{ IRGenerator inlines
template <typename... Args>
inline void IRGenerator::reportError(const std::string& fmt, Args&&... args) {
  char buf[1024];
  ssize_t n =
      snprintf(buf, sizeof(buf), fmt.c_str(), std::forward<Args>(args)...);

  if (n > 0) {
    reportError(buf);
  }
}
// }}}

//!@}

}  // namespace flow
}  // namespace xzero
