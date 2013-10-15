#pragma once

#include <x0/Api.h>
#include <x0/RegExp.h>
#include <x0/IPAddress.h>
#include <utility>
#include <memory>

namespace x0 {

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
class CastExpr;
template<typename> class LiteralExpr;
class FunctionCallExpr;
class VariableExpr;
class HandlerRefExpr;
class ListExpr;

typedef LiteralExpr<std::string> StringExpr;
typedef LiteralExpr<long long> NumberExpr;
typedef LiteralExpr<bool> BoolExpr;
typedef LiteralExpr<RegExp> RegExpExpr;
typedef LiteralExpr<IPAddress> IPAddressExpr;

class Stmt;
class ExprStmt;
class CompoundStmt;
class HandlerCallStmt;
class CondStmt;

class X0_API ASTVisitor
{
public:
	virtual ~ASTVisitor() {}

	// symbols
	virtual void visit(Variable& variable) = 0;
	virtual void visit(Handler& handler) = 0;
	virtual void visit(BuiltinFunction& symbol) = 0;
	virtual void visit(BuiltinHandler& symbol) = 0;
	virtual void visit(Unit& symbol) = 0;

	// expressions
	virtual void visit(UnaryExpr& expr) = 0;
	virtual void visit(BinaryExpr& expr) = 0;
	virtual void visit(CastExpr& expr) = 0;
	virtual void visit(FunctionCallExpr& expr) = 0;
	virtual void visit(VariableExpr& expr) = 0;
	virtual void visit(HandlerRefExpr& expr) = 0;
	virtual void visit(ListExpr& expr) = 0;

	virtual void visit(StringExpr& expr) = 0;
	virtual void visit(NumberExpr& expr) = 0;
	virtual void visit(BoolExpr& expr) = 0;
	virtual void visit(RegExpExpr& expr) = 0;
	virtual void visit(IPAddressExpr& expr) = 0;

	// statements
	virtual void visit(ExprStmt& stmt) = 0;
	virtual void visit(CompoundStmt& stmt) = 0;
	virtual void visit(CondStmt& stmt) = 0;
};

} // namespace x0
