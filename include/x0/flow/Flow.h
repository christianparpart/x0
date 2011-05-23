/* <flow/Flow.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_flow_Flow_h
#define sw_flow_Flow_h

#include <x0/flow/FlowToken.h>
#include <x0/flow/FlowLexer.h> // SourceLocation
#include <x0/RegExp.h>
#include <x0/Api.h>
#include <string>
#include <vector>

namespace x0 {

class Expr;
class UnaryExpr;
class BinaryExpr;
class ListExpr;
class CallExpr;
class VariableExpr;
class FunctionRefExpr;
template<class> class LiteralExpr;

class RegExp;
class IPAddress;

typedef LiteralExpr<bool> BoolExpr;
typedef LiteralExpr<long long> NumberExpr;
typedef LiteralExpr<std::string> StringExpr;
typedef LiteralExpr<RegExp> RegExpExpr;
typedef LiteralExpr<IPAddress> IPAddressExpr;

class Symbol;
class Variable;
class Function;
class Unit;

class Stmt;
class ExprStmt;
class CompoundStmt;
class CondStmt;

class ASTNode;
class ASTVisitor;

// {{{ visitors
class X0_API ASTVisitor // {{{
{
public:
	virtual ~ASTVisitor() {}

	// symbols
	virtual void visit(Variable& symbol) = 0;
	virtual void visit(Function& symbol) = 0;
	virtual void visit(Unit& symbol) = 0;

	// expressions
	virtual void visit(UnaryExpr& expr) = 0;
	virtual void visit(BinaryExpr& expr) = 0;
	virtual void visit(StringExpr& expr) = 0;
	virtual void visit(NumberExpr& expr) = 0;
	virtual void visit(BoolExpr& expr) = 0;
	virtual void visit(RegExpExpr& expr) = 0;
	virtual void visit(IPAddressExpr& expr) = 0;
	virtual void visit(VariableExpr& expr) = 0;
	virtual void visit(FunctionRefExpr& expr) = 0;
	virtual void visit(CallExpr& expr) = 0;
	virtual void visit(ListExpr& expr) = 0;

	// statements
	virtual void visit(ExprStmt& stmt) = 0;
	virtual void visit(CompoundStmt& stmt) = 0;
	virtual void visit(CondStmt& stmt) = 0;
}; // }}}
class X0_API FlowCallIterator : public ASTVisitor // {{{
{
public:
	typedef std::vector<CallExpr*> list_type;
	typedef list_type::iterator iterator;

private:
	list_type result_;
	iterator current_;

public:
	explicit FlowCallIterator(ASTNode* root = nullptr);
	~FlowCallIterator();

	void collect(ASTNode* root);
	size_t size() const;
	bool empty() const;

	CallExpr* operator*() { return *current_; }
	CallExpr* operator->() { return *current_; }

	FlowCallIterator& operator++();

	bool operator==(const FlowCallIterator& it) const;
	bool operator!=(const FlowCallIterator& it) const;

protected:
	virtual void visit(Variable& symbol);
	virtual void visit(Function& symbol);
	virtual void visit(Unit& symbol);

	// expressions
	virtual void visit(UnaryExpr& expr);
	virtual void visit(BinaryExpr& expr);
	virtual void visit(StringExpr& expr);
	virtual void visit(NumberExpr& expr);
	virtual void visit(BoolExpr& expr);
	virtual void visit(RegExpExpr& expr);
	virtual void visit(IPAddressExpr& expr);
	virtual void visit(VariableExpr& expr);
	virtual void visit(FunctionRefExpr& expr);
	virtual void visit(CallExpr& expr);
	virtual void visit(ListExpr& expr);

	// statements
	virtual void visit(ExprStmt& stmt);
	virtual void visit(CompoundStmt& stmt);
	virtual void visit(CondStmt& stmt);
}; // }}}
// }}}

// {{{ Operator & OperatorTraits
enum class Operator
{
	Undefined,

	// 1) unary
	UnaryPlus, UnaryMinus, Not,

	// 4) ext-rel binary
	Equal, UnEqual, Greater, Less, GreaterOrEqual, LessOrEqual, In,
	PrefixMatch, SuffixMatch, RegexMatch,

	// 14) add
	Plus, Minus, Or, Xor,

	// 18) mul
	Mul, Div, Mod, Shl, Shr, And,
	Pow,

	// 25) assign
	Assign,

	// 26) other
	Bracket, Paren, Is, As
};

struct X0_API OperatorTraits
{
	static bool isUnary(Operator op);
	static bool isBinary(Operator op);
	static bool isPrefix(Operator op);
	static const std::string& toString(Operator op);
};
// }}}

// {{{ SymbolTable
enum class Lookup
{
	Self            = 1,
	Parents         = 2,
	Outer           = 4,
	SelfAndParents  = 3,
	SelfAndOuter    = 5,
	OuterAndParents = 6,
	All             = 7
};

inline bool operator&(Lookup a, Lookup b)
{
	return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}

class X0_API SymbolTable
{
public:
	typedef std::vector<Symbol*> list_type;
	typedef list_type::iterator iterator;
	typedef list_type::const_iterator const_iterator;

private:
	list_type symbols_;
	std::vector<SymbolTable*> parents_;
	SymbolTable* outer_;

public:
	explicit SymbolTable(SymbolTable* outer = NULL);
	~SymbolTable();

	iterator begin();
	iterator end();

	void setOuterTable(SymbolTable* table);
	SymbolTable* outerTable() const;

	SymbolTable* appendParent(SymbolTable* table);
	SymbolTable* parentAt(size_t i) const;
	void removeParent(SymbolTable* table);
	size_t parentCount() const;

	Symbol* appendSymbol(Symbol* symbol);
	void removeSymbol(Symbol* symbol);
	Symbol* symbolAt(size_t i) const;
	size_t symbolCount() const;

	Symbol* lookup(const std::string& name, Lookup lookupMethod) const;
};
// }}}

// {{{ AST base
class X0_API ASTNode
{
protected:
	SourceLocation sourceLocation_;

public:
	ASTNode() {}
	ASTNode(const SourceLocation& sloc) : sourceLocation_(sloc) {}
	virtual ~ASTNode() {}

	SourceLocation& sourceLocation();
	void setSourceLocation(const SourceLocation& sloc);

	virtual void accept(ASTVisitor& v) = 0;
};
// }}}

// {{{ symbols
class X0_API Symbol : public ASTNode
{
public:
	enum Type {
		VARIABLE,
		FUNCTION,
		UNIT,
		TYPE
	};

private:
	Type type_;
	SymbolTable* scope_;
	std::string name_;

protected:
	Symbol(Type type, SymbolTable* scope, const std::string& name, const SourceLocation& sloc);

public:
	virtual ~Symbol() {}

	Type type() const;
	bool isVariable() const { return type_ == VARIABLE; }
	bool isFunction() const { return type_ == FUNCTION; }
	bool isUnit() const { return type_ == UNIT; }
	bool isType() const { return type_ == TYPE; }

	SymbolTable* parentScope() const;

	const std::string& name() const;
	void setName(const std::string& name);

	virtual void accept(ASTVisitor& v) = 0;
};

class X0_API Variable : public Symbol
{
private:
	Expr* value_;

public:
	Variable(const std::string& name, const SourceLocation& sloc); // external variable
	Variable(SymbolTable* scope, const std::string& name, Expr* value, const SourceLocation& sloc);
	~Variable();

	Expr* value() const;
	void setValue(Expr* value);

	virtual void accept(ASTVisitor& v);
};

/**
 * a function (internal or external)
 */
class X0_API Function : public Symbol
{
private:
	SymbolTable* scope_;
	Stmt* body_;
	bool isHandler_;
	FlowToken returnType_;
	std::vector<FlowToken> argTypes_;
	bool varArg_;

public:
	Function(const std::string& name);
	Function(const std::string& name, bool isHandler, const SourceLocation& sloc = SourceLocation());
	Function(SymbolTable* scope, const std::string& name, Stmt* body, bool isHandler, const SourceLocation& sloc);
	~Function();

	SymbolTable* scope() const;
	void setScope(SymbolTable *st);

	bool isHandler() const;
	void setIsHandler(bool);

	FlowToken returnType() const;
	void setReturnType(FlowToken);

	std::vector<FlowToken>& argTypes();
	const std::vector<FlowToken>& argTypes() const;

	bool isVarArg() const;
	void setIsVarArg(bool value);

	Stmt* body() const;
	void setBody(Stmt* body);

	virtual void accept(ASTVisitor& v);
};

class X0_API Unit : public Symbol
{
private:
	SymbolTable* members_;
	std::vector<std::pair<std::string, std::string> > imports_;

public:
	Unit();
	~Unit();

	SymbolTable& members() const;

	Symbol* insert(Symbol* symbol);
	Symbol* lookup(const std::string& name);
	Symbol* at(size_t i) const;
	size_t length() const;

	template<typename T>
	T* lookup(const std::string& name) { return dynamic_cast<T*>(lookup(name)); }

	// plugin import
	void import(const std::string& moduleName, const std::string& path);
	size_t importCount() const;
	const std::string& getImportName(size_t i) const;
	const std::string& getImportPath(size_t i) const; // an empty string if no custom path was passed.

	virtual void accept(ASTVisitor& v);
};
// }}}

// {{{ expr
class X0_API Expr : public ASTNode
{
public:
	Expr(const SourceLocation& sloc) : ASTNode(sloc) {}
};

class X0_API UnaryExpr : public Expr
{
private:
	Operator operator_;
	Expr* subExpr_;

public:
	UnaryExpr(Operator op, Expr* expr, const SourceLocation& sloc);
	~UnaryExpr();

	Operator operatorStyle() const;
	void setOperatorStyle(Operator op);

	Expr* subExpr() const;
	void setSubExpr(Expr* value);

	virtual void accept(ASTVisitor& v);
};

class X0_API BinaryExpr : public Expr
{
private:
	Operator operator_;
	Expr* left_;
	Expr* right_;

public:
	BinaryExpr(Operator op, Expr* left, Expr* right, const SourceLocation& sloc);
	~BinaryExpr();

	Operator operatorStyle() const;
	void setOperatorStyle(Operator op);

	Expr* leftExpr() const;
	Expr* rightExpr() const;

	virtual void accept(ASTVisitor& v);
};

template<typename T>
class X0_API LiteralExpr : public Expr
{
public:
	typedef T ValueType;

private:
	ValueType value_;

public:
	explicit LiteralExpr(const T& value, const SourceLocation& sloc) : Expr(sloc), value_(value) {}

	const ValueType& value() const { return value_; }
	void setValue(const ValueType& value) { value_ = value; }

	virtual void accept(ASTVisitor& v) { v.visit(*this); }
};

class X0_API CallExpr : public Expr
{
public:
	enum CallStyle
	{
		Undefined,
		Method,
		Assignment
	};

private:
	Function* callee_;
	ListExpr* args_;
	CallStyle callStyle_;

public:
	CallExpr(Function* callee, ListExpr* args, CallStyle, const SourceLocation& sloc);
	~CallExpr();

	Function* callee() const;
	ListExpr* args() const;
	CallStyle callStyle() const;

	virtual void accept(ASTVisitor& v);
};

class X0_API VariableExpr : public Expr
{
private:
	Variable* variable_;

public:
	VariableExpr(Variable* var, const SourceLocation& sloc);
	~VariableExpr();

	Variable* variable() const;
	void setVariable(Variable* var);

	virtual void accept(ASTVisitor& v);
};

class X0_API FunctionRefExpr : public Expr
{
private:
	Function* function_;

public:
	FunctionRefExpr(Function* ref, const SourceLocation& sloc);

	Function* function() const;
	void setFunction(Function* value);

	virtual void accept(ASTVisitor& v);
};

class X0_API ListExpr : public Expr
{
private:
	std::vector<Expr*> list_;

public:
	explicit ListExpr(const SourceLocation& sloc = SourceLocation());
	~ListExpr();

	bool empty() const;
	size_t size() const;
	void clear();

	void push_back(Expr* expr);
	int length() const;
	Expr* at(int i);

	void replaceAt(size_t i, Expr* expr);
	void replaceAll(Expr* expr);

	std::vector<Expr*>::iterator begin();
	std::vector<Expr*>::iterator end();

	std::vector<Expr*>::const_iterator begin() const;
	std::vector<Expr*>::const_iterator end() const;

	virtual void accept(ASTVisitor& v);
};
// }}}

// {{{ stmt
class X0_API Stmt : public ASTNode
{
public:
	Stmt(const SourceLocation& sloc) : ASTNode(sloc) {}
};

class X0_API ExprStmt : public Stmt
{
private:
	Expr* expression_;

public:
	ExprStmt(Expr* expr, const SourceLocation& sloc);
	~ExprStmt();

	Expr *expression() const;
	void setExpression(Expr *value);

	virtual void accept(ASTVisitor&);
};

class X0_API CompoundStmt : public Stmt
{
private:
	std::vector<Stmt*> statements_;

public:
	explicit CompoundStmt(const SourceLocation& sloc);
	~CompoundStmt();

	void push_back(Stmt *stmt);
	size_t length() const;
	Stmt* at(size_t index) const;

	std::vector<Stmt* >::iterator begin();
	std::vector<Stmt* >::iterator end();

	virtual void accept(ASTVisitor&);
};

class X0_API CondStmt : public Stmt
{
private:
	Expr* cond_;
	Stmt* thenStmt_;
	Stmt* elseStmt_;

public:
	CondStmt(Expr* cond, Stmt* thenStmt, Stmt* elseStmt, const SourceLocation& sloc);
	~CondStmt();

	Expr* condition() const;
	Stmt* thenStmt() const;
	Stmt* elseStmt() const;

	virtual void accept(ASTVisitor&);
};
// }}}

// {{{ inlines
// ASTNode
inline SourceLocation& ASTNode::sourceLocation()
{
	return sourceLocation_;
}

inline void ASTNode::setSourceLocation(const SourceLocation& sloc)
{
	sourceLocation_ = sloc;
}

// Symbol
inline Symbol::Type Symbol::type() const
{
	return type_;
}

inline SymbolTable *Symbol::parentScope() const
{
	return scope_;
}

// Unit
inline Symbol *Unit::at(size_t i) const
{
	return members_->symbolAt(i);
}

inline size_t Unit::length() const
{
	return members_->symbolCount();
}

// SymbolTable
inline SymbolTable::iterator SymbolTable::begin()
{
	return symbols_.begin();
}

inline SymbolTable::iterator SymbolTable::end()
{
	return symbols_.end();
}

// ListExpr
inline std::vector<Expr *>::iterator ListExpr::begin()
{
	return list_.begin();
}

inline std::vector<Expr *>::iterator ListExpr::end()
{
	return list_.end();
}

inline std::vector<Expr *>::const_iterator ListExpr::begin() const
{
	return list_.begin();
}

inline std::vector<Expr *>::const_iterator ListExpr::end() const
{
	return list_.end();
}

// CompoundStmt
inline std::vector<Stmt *>::iterator CompoundStmt::begin()
{
	return statements_.begin();
}

inline std::vector<Stmt *>::iterator CompoundStmt::end()
{
	return statements_.end();
}
// }}}

} // namespace x0

#endif
