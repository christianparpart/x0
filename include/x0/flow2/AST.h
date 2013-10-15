#pragma once

#include <x0/flow2/FlowType.h>
#include <x0/flow2/FlowLocation.h>
#include <x0/flow2/ASTVisitor.h>
#include <x0/RegExp.h>
#include <x0/Api.h>
#include <utility>
#include <memory>
#include <vector>

namespace x0 {

class ASTVisitor;
class FlowBackend;

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

class X0_API ASTNode // {{{
{
protected:
	FlowLocation location_;

public:
	ASTNode() {}
	ASTNode(const FlowLocation& sloc) : location_(sloc) {}
	virtual ~ASTNode() {}

	const FlowLocation& location() const { return location_; }
	void setLocation(const FlowLocation& sloc) { location_ = sloc; }

	virtual void accept(ASTVisitor& v) = 0;
};
// }}}
// {{{ Symbols
class X0_API Symbol : public ASTNode {
	std::string name_;

protected:
	Symbol(const std::string& name, const FlowLocation& loc);

public:
	const std::string& name() const { return name_; }
	void setName(const std::string& value) { name_ = value; }

	virtual void accept(ASTVisitor& v);
};

class X0_API SymbolTable {
	Symbol* find(const std::string& name, Lookup lookupMethod) const;
};

class X0_API ScopedSymbol : public Symbol {
protected:
	SymbolTable scope_;

protected:
	ScopedSymbol(const std::string& name, const FlowLocation& loc);

public:
	SymbolTable* scope() const { return &scope_; }

	void setParentScope(SymbolTable* scope);
};

class X0_API Variable : public Symbol {
	std::unique_ptr<Expr> initializer_;
public:
	Variable();
	Variable(const std::unique_ptr<Expr>& value);

	Expr* initializer() const { return initializer_.get(); }
	void setInitializer(std::unique_ptr<Expr>&& value) { initializer_ = std::move(value); }

	virtual void accept(ASTVisitor& v);
};

class X0_API Handler : public ScopedSymbol {
private:
	std::unique_ptr<Stmt> body_;

public:
	Handler(std::unique_ptr<Stmt>&& body) :
		body_(std::move(body)) {}

	virtual void accept(ASTVisitor& v);
};

class X0_API BuiltinFunction : public Symbol {
private:
	FlowBackend* owner_;
	FlowType returnType_;
	std::vector<FlowType> signature_;

public:
	BuiltinFunction(FlowBackend* owner, FlowType rt);

	FlowBackend* owner() const { return owner_; }

	FlowType returnType() const { return returnType_; }
	void setReturnType(FlowType type) { returnType_ = type; }

	const std::vector<FlowType>& signature() const { return signature_; }
	std::vector<FlowType>& signature() { return signature_; }

	virtual void accept(ASTVisitor& v);
};

class X0_API BuiltinHandler : public Symbol {
private:
	FlowBackend* owner_;

public:
	BuiltinHandler(FlowBackend* owner, const std::string& name, const FlowLocation& loc) :
		Symbol(name, loc),
		owner_(owner),

	virtual void accept(ASTVisitor& v);
};

class X0_API Unit : public Symbol {
	std::list<Variable*> globals_;
	std::list<Handler*> handlers_;
};
// }}}
// {{{ Expr
class X0_API Expr : public ASTNode {
protected:
	explicit Expr(const FlowLocation&);
};
class X0_API UnaryExpr : public Expr {
};

class X0_API BinaryExpr : public Expr {
	std::unique_ptr<Expr> lhs_;
	std::unique_ptr<Expr> rhs_;

private:
	BinaryExpr(Expr* lhs, Expr* rhs);
};

template<typename T>
class X0_API LiteralExpr : public Expr {
private:
	T value_;

public:
	explicit LiteralExpr(const T& value, const FlowLocation& sloc) :
		Expr(sloc), value_(value) {}

	const T& value() const { return value_; }
	void setValue(const T& value) { value_ = value; }

	virtual void accept(ASTVisitor& v) { v.visit(*this); }
};

class X0_API CastExpr : public UnaryExpr
{
private:
	FlowType targetType_;

public:
	CastExpr(FlowType targetType, std::unique_ptr<Expr> subExpr, const FlowLocation& loc);
	~CastExpr();

	FlowType targetType() const { return targetType_; }
	void setTargetType(FlowType t) { targetType_ = t; }

	virtual void accept(ASTVisitor& v);
};

class X0_API FunctionCallExpr : public Expr {
	BuiltinFunction* callee_;
	std::unique_ptr<ListExpr> args_;

public:
	BuiltinFunction* callee() const { return callee_; }
	ListExpr* args() const { return args_.get(); }

	virtual void accept(ASTVisitor& v);
};

class X0_API VariableExpr : public Expr
{
private:
	Variable* variable_;

public:
	VariableExpr(Variable* var, const FlowLocation& sloc);
	~VariableExpr();

	Variable* variable() const;
	void setVariable(Variable* var);

	virtual void accept(ASTVisitor& v);
};

class X0_API HandlerRefExpr : public Expr
{
private:
	Handler* function_;

public:
	HandlerRefExpr(Handler* ref, const FlowLocation& sloc);

	Handler* handler() const;
	void setHandler(Handler* handler);

	virtual void accept(ASTVisitor& v);
};

class X0_API ListExpr : public Expr
{
private:
	std::vector<std::unique_ptr<Expr>> list_;

public:
	ListExpr();
	~ListExpr();

	void push_back(Expr* expr);
	Expr* back() { return !list_.empty() ? list_.back().get() : nullptr; }
	size_t size() const;
	Expr* at(int i);
	bool empty() const;
	void clear();

	void replaceAt(size_t i, Expr* expr);
	void replaceAll(Expr* expr);

	std::vector<Expr*>::iterator begin();
	std::vector<Expr*>::iterator end();

	std::vector<Expr*>::const_iterator begin() const;
	std::vector<Expr*>::const_iterator end() const;

	virtual void accept(ASTVisitor& v);
};
// }}}
// {{{ Stmt
class X0_API Stmt : public ASTNode
{
protected:
	explicit Stmt(const FlowLocation& sloc) : ASTNode(sloc) {}
};

class X0_API ExprStmt : public Stmt
{
private:
	std::unique_ptr<Expr> expression_;

public:
	ExprStmt(std::unique_ptr<Expr> expr, const FlowLocation& sloc);
	~ExprStmt();

	Expr *expression() const;
	void setExpression(std::unique_ptr<Expr> value);

	virtual void accept(ASTVisitor&);
};

class X0_API CompoundStmt : public Stmt
{
private:
	std::vector<std::unique_ptr<Stmt>> statements_;

public:
	explicit CompoundStmt(const FlowLocation& sloc);
	~CompoundStmt();

	void push_back(std::unique_ptr<Stmt> stmt);
	size_t length() const;
	Stmt* at(size_t index) const;

	std::vector<Stmt*>::iterator begin();
	std::vector<Stmt*>::iterator end();

	virtual void accept(ASTVisitor&);
};

class X0_API HandlerCallStmt : public Stmt {
	Handler* handler_;

public:
	virtual void accept(ASTVisitor&);
};

class X0_API CondStmt : public Stmt
{
private:
	std::unique_ptr<Expr> cond_;
	std::unique_ptr<Stmt> thenStmt_;
	std::unique_ptr<Stmt> elseStmt_;

public:
	CondStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> thenStmt,
			std::unique_ptr<Stmt> elseStmt, const FlowLocation& sloc);
	~CondStmt();

	Expr* condition() const;
	Stmt* thenStmt() const;
	Stmt* elseStmt() const;

	virtual void accept(ASTVisitor&);
};
// }}}

} // namespace x0
