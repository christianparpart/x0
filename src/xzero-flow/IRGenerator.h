// This file is part of the "x0" project, http://github.com/christianparpart/x0>
//   (c) 2009-2017 Christian Parpart <christian@parpart.family>
//
// Licensed under the MIT License (the "License"); you may not use this
// file except in compliance with the License. You may obtain a copy of
// the License at: http://opensource.org/licenses/MIT

#pragma once

#include <xzero/defines.h>
#include <xzero-flow/ASTVisitor.h>
#include <xzero-flow/ir/IRBuilder.h>
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
  IRGenerator();
  ~IRGenerator();

  static std::unique_ptr<IRProgram> generate(
      UnitSym* unit, const std::vector<std::string>& exportedHandlers);

  void setErrorCallback(std::function<void(const std::string&)>&& handler) {
    onError_ = std::move(handler);
  }

  void setExports(const std::vector<std::string>& exports) {
    exports_ = exports;
  }

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
  virtual void accept(AssignStmt& stmt);

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
