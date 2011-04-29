/* <flow/Flow.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/flow/Flow.h>
#include <algorithm>

#include <cassert>

namespace x0 {

// {{{ OperatorTraits
bool OperatorTraits::isUnary(Operator op)
{
	switch (op) {
	case Operator::UnaryPlus:
	case Operator::UnaryMinus:
	case Operator::Not:
		return true;
	default:
		return false;
	}
}

bool OperatorTraits::isBinary(Operator op)
{
	switch (op) {
	case Operator::Equal:
	case Operator::UnEqual:
	case Operator::Greater:
	case Operator::Less:
	case Operator::GreaterOrEqual:
	case Operator::LessOrEqual:
	case Operator::In:
	case Operator::PrefixMatch:
	case Operator::SuffixMatch:
	case Operator::RegexMatch:
		return true;
	default:
		return false;
	}
}

bool OperatorTraits::isPrefix(Operator op)
{
	return !isUnary(op) && op != Operator::Paren && op != Operator::Bracket;
}

const std::string& OperatorTraits::toString(Operator op)
{
	static std::string values[] = {
		"UNDEFINED",
		"+", "-", "!",
		"==", "!=", ">", "<", ">=", "<=", "in",
		"=^", "=$", "=~",
		"+", "-", "||", "^",
		"*", "/", "%", "<<", ">>", "&",
		"=",
		"[]", "()", "is", "as"
	};
	return values[static_cast<size_t>(op)];
}
// }}}

// {{{ SymbolTable
SymbolTable::SymbolTable(SymbolTable *outer) :
	symbols_(),
	parents_(),
	outer_(outer)
{
}

SymbolTable::~SymbolTable()
{
	for (auto i = parents_.begin(), e = parents_.end(); i != e; ++i)
		delete *i;

	for (auto i = symbols_.begin(), e = symbols_.end(); i != e; ++i)
		delete *i;
}

void SymbolTable::setOuterTable(SymbolTable *outer)
{
	outer_ = outer;
}

SymbolTable *SymbolTable::outerTable() const
{
	return outer_;
}

SymbolTable *SymbolTable::appendParent(SymbolTable *table)
{
	parents_.push_back(table);
	return table;
}

SymbolTable *SymbolTable::parentAt(size_t i) const
{
	return parents_[i];
}

void SymbolTable::removeParent(SymbolTable *table)
{
}

size_t SymbolTable::parentCount() const
{
	return parents_.size();
}

Symbol *SymbolTable::appendSymbol(Symbol *symbol)
{
	symbols_.push_back(symbol);
	return symbol;
}

void SymbolTable::removeSymbol(Symbol *symbol)
{
	auto i = std::find(symbols_.begin(), symbols_.end(), symbol);

	if (i != symbols_.end())
		symbols_.erase(i);
}

Symbol *SymbolTable::symbolAt(size_t i) const
{
	return symbols_[i];
}

size_t SymbolTable::symbolCount() const
{
	return symbols_.size();
}

Symbol *SymbolTable::lookup(const std::string& name, Lookup method) const
{
	// search local
	if (method & Lookup::Self)
		for (auto i = symbols_.begin(), e = symbols_.end(); i != e; ++i)
			if ((*i)->name() == name)
				return *i;

	// search parents
	if (method & Lookup::Parents)
		for (auto i = parents_.begin(), e = parents_.end(); i != e; ++i)
			if (Symbol *result = (*i)->lookup(name, method))
				return result;

	// search outer
	if (method & Lookup::Outer)
		if (outer_)
			return outer_->lookup(name, method);

	return NULL;
}
// }}}

// {{{ symbols
// Symbol
Symbol::Symbol(Type type, SymbolTable *scope, const std::string& name, const SourceLocation& sloc) :
	type_(type), scope_(scope), name_(name)
{
	setSourceLocation(sloc);
}

const std::string& Symbol::name() const
{
	return name_;
}

void Symbol::setName(const std::string& name)
{
	name_ = name;
}

// Variable
Variable::Variable(const std::string& name, const SourceLocation& sloc) :
	Symbol(VARIABLE, NULL, name, sloc), value_(NULL)
{
}

Variable::Variable(SymbolTable *scope, const std::string& name, Expr *value, const SourceLocation& sloc) :
	Symbol(VARIABLE, scope, name, sloc), value_(value)
{
}

Variable::~Variable()
{
	setValue(NULL);
}

Expr *Variable::value() const
{
	return value_;
}

void Variable::setValue(Expr *value)
{
	if (value_)
		delete value_;

	value_ = value;
}

void Variable::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// Function
Function::Function(const std::string& name) :
	Symbol(FUNCTION, NULL, name, SourceLocation()),
	scope_(NULL),
	body_(NULL),
	isHandler_(false),
	returnType_(FlowToken::Void),
	argTypes_(),
	varArg_(false)
{
}

Function::Function(const std::string& name, bool isHandler, const SourceLocation& sloc) :
	Symbol(FUNCTION, NULL, name, sloc),
	scope_(NULL),
	body_(NULL),
	isHandler_(isHandler),
	returnType_(FlowToken::Void),
	argTypes_(),
	varArg_(false)
{
	if (isHandler_) {
		setReturnType(FlowToken::Boolean);
	}
}

Function::Function(SymbolTable *scope, const std::string& name, Stmt *body, bool isHandler, const SourceLocation& sloc) :
	Symbol(FUNCTION, scope ? scope->outerTable() : NULL, name, sloc),
	scope_(scope),
	body_(body),
	isHandler_(isHandler),
	returnType_(FlowToken::Void),
	argTypes_(),
	varArg_(false)
{
	if (isHandler_) {
		setReturnType(FlowToken::Boolean);
	}
}

Function::~Function()
{
	if (scope_)
		delete scope_;

	setBody(NULL);
}

SymbolTable *Function::scope() const
{
	return scope_;
}

void Function::setScope(SymbolTable *value)
{
	if (scope_ != value && scope_)
		delete scope_;

	scope_ = value;
}

bool Function::isHandler() const
{
	return isHandler_;
}

void Function::setIsHandler(bool value)
{
	if (value)
		returnType_ = FlowToken::Boolean;

	isHandler_ = value;
}

FlowToken Function::returnType() const
{
	return returnType_;
}

void Function::setReturnType(FlowToken t)
{
	assert(FlowTokenTraits::isType(t));
	returnType_ = t;
}

std::vector<FlowToken> *Function::argTypes()
{
	return &argTypes_;
}

bool Function::isVarArg() const
{
	return varArg_;
}

void Function::setIsVarArg(bool value)
{
	varArg_ = value;
}

Stmt *Function::body() const
{
	return body_;
}

void Function::setBody(Stmt *body)
{
	if (body_)
		delete body_;

	body_ = body;
}

void Function::accept(ASTVisitor& v)
{
	v.visit(*this);
}

Unit::Unit() :
	Symbol(UNIT, NULL, "#unit", SourceLocation()),
	members_(NULL)
{
	members_ = new SymbolTable();
}

Unit::~Unit()
{
	delete members_;
}

SymbolTable *Unit::members() const
{
	return members_;
}

Symbol *Unit::insert(Symbol *symbol)
{
	members_->appendSymbol(symbol);
	return symbol;
}

Symbol *Unit::lookup(const std::string& name)
{
	return members_->lookup(name, Lookup::All);
}

void Unit::import(const std::string& moduleName, const std::string& path)
{
	imports_.push_back(std::make_pair(moduleName, path));
}

size_t Unit::importCount() const
{
	return imports_.size();
}

const std::string& Unit::getImportName(size_t i) const
{
	return imports_[i].first;
}

const std::string& Unit::getImportPath(size_t i) const
{
	return imports_[i].second;
}


void Unit::accept(ASTVisitor& v)
{
	v.visit(*this);
}
// }}}

// {{{ expr
// UnaryExpr
UnaryExpr::UnaryExpr(Operator op, Expr *expr, const SourceLocation& sloc) :
	Expr(sloc),
	operator_(op), subExpr_(expr)
{
}

UnaryExpr::~UnaryExpr()
{
	setSubExpr(NULL);
}

Operator UnaryExpr::operatorStyle() const
{
	return operator_;
}

void UnaryExpr::setOperatorStyle(Operator op)
{
	operator_ = op;
}

Expr *UnaryExpr::subExpr() const
{
	return subExpr_;
}

void UnaryExpr::setSubExpr(Expr *value)
{
	if (subExpr_)
		delete subExpr_;

	subExpr_ = value;
}

void UnaryExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// BinaryExpr
BinaryExpr::BinaryExpr(Operator op, Expr *left, Expr *right, const SourceLocation& sloc) :
	Expr(sloc),
	operator_(op), left_(left), right_(right)
{
}

BinaryExpr::~BinaryExpr()
{
	if (left_)
		delete left_;

	if (right_)
		delete right_;
}

Operator BinaryExpr::operatorStyle() const
{
	return operator_;
}

void BinaryExpr::setOperatorStyle(Operator op)
{
	operator_ = op;
}

Expr *BinaryExpr::leftExpr() const
{
	return left_;
}

Expr *BinaryExpr::rightExpr() const
{
	return right_;
}

void BinaryExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}

ListExpr::ListExpr(const SourceLocation& sloc) :
	Expr(sloc),
	list_()
{
}

ListExpr::~ListExpr()
{
	for (auto i = list_.begin(), e = list_.end(); i != e; ++i)
		delete *i;
}

void ListExpr::push_back(Expr *expr)
{
	list_.push_back(expr);
}

int ListExpr::length() const
{
	return list_.size();
}

Expr *ListExpr::at(int i)
{
	return list_[i];
}

void ListExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// CallExpr
CallExpr::CallExpr(Function *callee, ListExpr *args, CallStyle cs, const SourceLocation& sloc) :
	Expr(sloc), callee_(callee), args_(args), callStyle_(cs)
{
}

CallExpr::~CallExpr()
{
	if (args_)
		delete args_;
}

Function *CallExpr::callee() const
{
	return callee_;
}

ListExpr *CallExpr::args() const
{
	return args_;
}

CallExpr::CallStyle CallExpr::callStyle() const
{
	return callStyle_;
}

void CallExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}

VariableExpr::VariableExpr(Variable *var, const SourceLocation& sloc) :
	Expr(sloc), variable_(var)
{
}

VariableExpr::~VariableExpr()
{
}

Variable *VariableExpr::variable() const
{
	return variable_;
}

void VariableExpr::setVariable(Variable *var)
{
	variable_ = var;
}

void VariableExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// FunctionRefExpr
FunctionRefExpr::FunctionRefExpr(Function *ref, const SourceLocation& sloc) :
	Expr(sloc),
	function_(ref)
{
}

Function *FunctionRefExpr::function() const
{
	return function_;
}

void FunctionRefExpr::setFunction(Function *value)
{
	function_ = value;
}

void FunctionRefExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}
// }}}

// {{{ stmt
// ExprStmt
ExprStmt::ExprStmt(Expr *expr, const SourceLocation& sloc) :
	Stmt(sloc), expression_(expr)
{
}

ExprStmt::~ExprStmt()
{
	if (expression_)
		delete expression_;
}

Expr *ExprStmt::expression() const
{
	return expression_;
}

void ExprStmt::setExpression(Expr *value)
{
	if (expression_)
		delete expression_;

	expression_ = value;
}

void ExprStmt::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// CompoundStmt
CompoundStmt::CompoundStmt(const SourceLocation& sloc) :
	Stmt(sloc), statements_()
{
}

CompoundStmt::~CompoundStmt()
{
	for (auto i = statements_.begin(), e = statements_.end(); i != e; ++i)
		delete *i;
}

void CompoundStmt::push_back(Stmt *stmt)
{
	statements_.push_back(stmt);
}

size_t CompoundStmt::length() const
{
	return statements_.size();
}

Stmt *CompoundStmt::at(size_t index) const
{
	return statements_[index];
}

void CompoundStmt::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// CondStmt
CondStmt::CondStmt(Expr *cond, Stmt *thenStmt, Stmt *elseStmt, const SourceLocation& sloc) :
	Stmt(sloc),
	cond_(cond),
	thenStmt_(thenStmt),
	elseStmt_(elseStmt)
{
}

CondStmt::~CondStmt()
{
	if (cond_)
		delete cond_;

	if (thenStmt_)
		delete thenStmt_;

	if (elseStmt_)
		delete elseStmt_;
}

Expr *CondStmt::condition() const
{
	return cond_;
}

Stmt *CondStmt::thenStmt() const
{
	return thenStmt_;
}

Stmt *CondStmt::elseStmt() const
{
	return elseStmt_;
}

void CondStmt::accept(ASTVisitor& v)
{
	v.visit(*this);
}
// }}}

} // namespace x0
