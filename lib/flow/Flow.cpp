/* <flow/Flow.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2010-2013 Christian Parpart <trapni@gmail.com>
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
	case Operator::Cast:
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
		"+", "-", "!", "cast",
		"==", "!=", ">", "<", ">=", "<=", "in",
		"=^", "=$", "=~",
		"+", "-", "||", "^",
		"*", "/", "%", "<<", ">>", "&",
		"=",
		"[]", "()", "is"
	};
	return values[static_cast<size_t>(op)];
}
// }}}

// {{{ SymbolTable
SymbolTable::SymbolTable(SymbolTable* outer) :
	symbols_(),
	parents_(),
	outer_(outer)
{
}

SymbolTable::~SymbolTable()
{
	for (auto parent: parents_)
		delete parent;

	for (auto symbol: symbols_)
		delete symbol;
}

void SymbolTable::setOuterTable(SymbolTable* outer)
{
	outer_ = outer;
}

SymbolTable* SymbolTable::outerTable() const
{
	return outer_;
}

SymbolTable* SymbolTable::appendParent(SymbolTable* table)
{
	parents_.push_back(table);
	return table;
}

SymbolTable* SymbolTable::parentAt(size_t i) const
{
	return parents_[i];
}

void SymbolTable::removeParent(SymbolTable* table)
{
}

size_t SymbolTable::parentCount() const
{
	return parents_.size();
}

Symbol* SymbolTable::appendSymbol(Symbol* symbol)
{
	symbols_.push_back(symbol);
	return symbol;
}

void SymbolTable::removeSymbol(Symbol* symbol)
{
	auto i = std::find(symbols_.begin(), symbols_.end(), symbol);

	if (i != symbols_.end())
		symbols_.erase(i);
}

Symbol* SymbolTable::symbolAt(size_t i) const
{
	return symbols_[i];
}

size_t SymbolTable::symbolCount() const
{
	return symbols_.size();
}

Symbol* SymbolTable::lookup(const std::string& name, Lookup method) const
{
	// search local
	if (method & Lookup::Self)
		for (auto symbol: symbols_)
			if (symbol->name() == name)
				return symbol;

	// search parents
	if (method & Lookup::Parents)
		for (auto parent: parents_)
			if (Symbol* result = parent->lookup(name, method))
				return result;

	// search outer
	if (method & Lookup::Outer)
		if (outer_)
			return outer_->lookup(name, method);

	return nullptr;
}
// }}}

// {{{ symbols
// Symbol
Symbol::Symbol(Type type, SymbolTable* scope, const std::string& name, const SourceLocation& sloc) :
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
	Symbol(VARIABLE, nullptr, name, sloc), value_(nullptr)
{
}

Variable::Variable(SymbolTable* scope, const std::string& name, Expr* value, const SourceLocation& sloc) :
	Symbol(VARIABLE, scope, name, sloc), value_(value)
{
}

Variable::~Variable()
{
	setValue(nullptr);
}

Expr* Variable::value() const
{
	return value_;
}

void Variable::setValue(Expr* value)
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
	Symbol(FUNCTION, nullptr, name, SourceLocation()),
	scope_(nullptr),
	body_(nullptr),
	isHandler_(false),
	returnType_(FlowToken::VoidType),
	argTypes_(),
	varArg_(false)
{
}

Function::Function(const std::string& name, bool isHandler, const SourceLocation& sloc) :
	Symbol(FUNCTION, nullptr, name, sloc),
	scope_(nullptr),
	body_(nullptr),
	isHandler_(isHandler),
	returnType_(FlowToken::VoidType),
	argTypes_(),
	varArg_(false)
{
	if (isHandler_) {
		setReturnType(FlowToken::BoolType);
	}
}

Function::Function(SymbolTable* scope, const std::string& name, Stmt* body, bool isHandler, const SourceLocation& sloc) :
	Symbol(FUNCTION, scope ? scope->outerTable() : nullptr, name, sloc),
	scope_(scope),
	body_(body),
	isHandler_(isHandler),
	returnType_(FlowToken::VoidType),
	argTypes_(),
	varArg_(false)
{
	if (isHandler_) {
		setReturnType(FlowToken::BoolType);
	}
}

Function::~Function()
{
	if (scope_)
		delete scope_;

	setBody(nullptr);
}

SymbolTable* Function::scope() const
{
	return scope_;
}

void Function::setScope(SymbolTable* value)
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

std::vector<FlowToken>& Function::argTypes()
{
	return argTypes_;
}

const std::vector<FlowToken>& Function::argTypes() const
{
	return argTypes_;
}

bool Function::isVarArg() const
{
	return varArg_;
}

void Function::setIsVarArg(bool value)
{
	varArg_ = value;
}

Stmt* Function::body() const
{
	return body_;
}

void Function::setBody(Stmt* body)
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
	Symbol(UNIT, nullptr, "#unit", SourceLocation()),
	members_(nullptr)
{
	members_ = new SymbolTable();
}

Unit::~Unit()
{
	delete members_;
}

SymbolTable& Unit::members() const
{
	return *members_;
}

Symbol* Unit::insert(Symbol* symbol)
{
	members_->appendSymbol(symbol);
	return symbol;
}

Symbol* Unit::lookup(const std::string& name)
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
UnaryExpr::UnaryExpr(Operator op, Expr* expr, const SourceLocation& sloc) :
	Expr(sloc),
	operator_(op), subExpr_(expr)
{
}

UnaryExpr::~UnaryExpr()
{
	setSubExpr(nullptr);
}

Operator UnaryExpr::operatorStyle() const
{
	return operator_;
}

void UnaryExpr::setOperatorStyle(Operator op)
{
	operator_ = op;
}

Expr* UnaryExpr::subExpr() const
{
	return subExpr_;
}

void UnaryExpr::setSubExpr(Expr* value)
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
BinaryExpr::BinaryExpr(Operator op, Expr* left, Expr* right, const SourceLocation& sloc) :
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

Expr* BinaryExpr::leftExpr() const
{
	return left_;
}

Expr* BinaryExpr::rightExpr() const
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
	clear();
}

bool ListExpr::empty() const
{
	return list_.empty();
}

size_t ListExpr::size() const
{
	return list_.size();
}

void ListExpr::clear()
{
	for (auto i: list_)
		delete i;

	list_.clear();
}

void ListExpr::push_back(Expr* expr)
{
	list_.push_back(expr);
}

int ListExpr::length() const
{
	return list_.size();
}

Expr* ListExpr::at(int i)
{
	return list_[i];
}

void ListExpr::replaceAt(size_t i, Expr* e)
{
	delete list_[i];
	list_[i] = e;
}

void ListExpr::replaceAll(Expr* e)
{
	clear();
	push_back(e);
}

void ListExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// CastExpr
CastExpr::CastExpr(FlowValue::Type targetType, Expr* subExpr, const SourceLocation& sloc) :
	UnaryExpr(Operator::Cast, subExpr, sloc),
	targetType_(targetType)
{
}

CastExpr::~CastExpr()
{
}

void CastExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// CallExpr
CallExpr::CallExpr(Function* callee, ListExpr* args, CallStyle cs, const SourceLocation& sloc) :
	Expr(sloc),
	callee_(callee),
	args_(args ? args : new ListExpr()),
	callStyle_(cs)
{
}

CallExpr::~CallExpr()
{
	if (args_)
		delete args_;
}

Function* CallExpr::callee() const
{
	return callee_;
}

ListExpr* CallExpr::args() const
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

VariableExpr::VariableExpr(Variable* var, const SourceLocation& sloc) :
	Expr(sloc), variable_(var)
{
}

VariableExpr::~VariableExpr()
{
}

Variable* VariableExpr::variable() const
{
	return variable_;
}

void VariableExpr::setVariable(Variable* var)
{
	variable_ = var;
}

void VariableExpr::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// FunctionRefExpr
FunctionRefExpr::FunctionRefExpr(Function* ref, const SourceLocation& sloc) :
	Expr(sloc),
	function_(ref)
{
}

Function* FunctionRefExpr::function() const
{
	return function_;
}

void FunctionRefExpr::setFunction(Function* value)
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
ExprStmt::ExprStmt(Expr* expr, const SourceLocation& sloc) :
	Stmt(sloc), expression_(expr)
{
}

ExprStmt::~ExprStmt()
{
	if (expression_)
		delete expression_;
}

Expr* ExprStmt::expression() const
{
	return expression_;
}

void ExprStmt::setExpression(Expr* value)
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
	for (auto stmt: statements_)
		delete stmt;
}

void CompoundStmt::push_back(Stmt* stmt)
{
	statements_.push_back(stmt);
}

size_t CompoundStmt::length() const
{
	return statements_.size();
}

Stmt* CompoundStmt::at(size_t index) const
{
	return statements_[index];
}

void CompoundStmt::accept(ASTVisitor& v)
{
	v.visit(*this);
}

// CondStmt
CondStmt::CondStmt(Expr* cond, Stmt* thenStmt, Stmt* elseStmt, const SourceLocation& sloc) :
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

Expr* CondStmt::condition() const
{
	return cond_;
}

Stmt* CondStmt::thenStmt() const
{
	return thenStmt_;
}

Stmt* CondStmt::elseStmt() const
{
	return elseStmt_;
}

void CondStmt::accept(ASTVisitor& v)
{
	v.visit(*this);
}
// }}}

// {{{ FlowCallIterator
FlowCallIterator::FlowCallIterator(ASTNode* root) :
	result_(),
	current_()
{
	if (root)
		collect(root);
}

FlowCallIterator::~FlowCallIterator()
{
}

void FlowCallIterator::collect(ASTNode* root)
{
	root->accept(*this);
	current_ = result_.begin();
}

size_t FlowCallIterator::size() const
{
	return result_.size();
}

bool FlowCallIterator::empty() const
{
	return result_.empty();
}

FlowCallIterator& FlowCallIterator::operator++()
{
	++current_;
	return *this;
}

bool FlowCallIterator::operator==(const FlowCallIterator& it) const
{
	if (this == &it)
		return true;

	if (current_ == it.current_)
		return true;

	if (current_ == result_.end() && it.empty())
		return true;

	return false;
}

bool FlowCallIterator::operator!=(const FlowCallIterator& it) const
{
	return !(*this == it);
}

void FlowCallIterator::visit(Variable& variable)
{
	if (variable.value())
		variable.value()->accept(*this);
}

void FlowCallIterator::visit(Function& function)
{
	if (function.scope())
		for (auto sym: *function.scope())
			sym->accept(*this);

	if (function.body())
		function.body()->accept(*this);
}

void FlowCallIterator::visit(Unit& unit)
{
	for (auto s: unit.members())
		s->accept(*this);
}

void FlowCallIterator::visit(UnaryExpr& expr)
{
	expr.subExpr()->accept(*this);
}

void FlowCallIterator::visit(BinaryExpr& expr)
{
	expr.leftExpr()->accept(*this);
	expr.rightExpr()->accept(*this);
}

void FlowCallIterator::visit(StringExpr& expr)
{
	// nothing
}

void FlowCallIterator::visit(NumberExpr& expr)
{
	// nothing
}

void FlowCallIterator::visit(BoolExpr& expr)
{
	// nothing
}

void FlowCallIterator::visit(RegExpExpr& expr)
{
	// nothing
}

void FlowCallIterator::visit(IPAddressExpr& expr)
{
	// nothing
}

void FlowCallIterator::visit(VariableExpr& expr)
{
	// nothing
}

void FlowCallIterator::visit(FunctionRefExpr& expr)
{
	// nothing
}

void FlowCallIterator::visit(CastExpr& expr)
{
	// nothing
}

void FlowCallIterator::visit(CallExpr& call)
{
	call.args()->accept(*this);
	call.callee()->accept(*this);

	result_.push_back(&call);
}

void FlowCallIterator::visit(ListExpr& listExpr)
{
	for (auto expr: listExpr)
		expr->accept(*this);
}

void FlowCallIterator::visit(ExprStmt& stmt)
{
	stmt.expression()->accept(*this);
}

void FlowCallIterator::visit(CompoundStmt& compoundStmt)
{
	for (auto stmt: compoundStmt)
		stmt->accept(*this);
}

void FlowCallIterator::visit(CondStmt& stmt)
{
	stmt.condition()->accept(*this);
	stmt.thenStmt()->accept(*this);

	if (stmt.elseStmt())
		stmt.elseStmt()->accept(*this);
}
// }}}

} // namespace x0
