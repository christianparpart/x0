#pragma once

#include <x0/Api.h>
#include <x0/RegExp.h>
#include <x0/IPAddress.h>
#include <x0/Cidr.h>
#include <utility>
#include <memory>

namespace x0 {

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
template<typename> class LiteralExpr;
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
class AssignStmt;

class X0_API ASTVisitor
{
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
	virtual void accept(AssignStmt& stmt) = 0;
};

//!@}

} // namespace x0
