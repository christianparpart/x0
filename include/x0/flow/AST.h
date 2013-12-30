#pragma once

#include <x0/flow/vm/Signature.h>
#include <x0/flow/vm/Instruction.h> // Opcode
#include <x0/flow/FlowType.h>
#include <x0/flow/FlowToken.h>
#include <x0/flow/FlowLocation.h>
#include <x0/flow/ASTVisitor.h>
#include <x0/RegExp.h>
#include <x0/Api.h>
#include <utility>
#include <memory>
#include <vector>
#include <list>

namespace x0 {

namespace FlowVM {
    class NativeCallback;
}

class ASTVisitor;
class FlowBackend;
class SymbolTable;
class Expr;

class X0_API ASTNode // {{{
{
protected:
	FlowLocation location_;

public:
	ASTNode() {}
	ASTNode(const FlowLocation& loc) : location_(loc) {}
	virtual ~ASTNode() {}

	FlowLocation& location() { return location_; }
	const FlowLocation& location() const { return location_; }
	void setLocation(const FlowLocation& loc) { location_ = loc; }

	virtual void visit(ASTVisitor& v) = 0;
};
// }}}
// {{{ Symbols
class X0_API Symbol : public ASTNode {
public:
	enum Type {
		Variable = 1,
		Handler,
		BuiltinFunction,
		BuiltinHandler,
		Unit,
	};

private:
	Type type_;
	std::string name_;

protected:
	friend class SymbolTable;
	SymbolTable* owner_;

	Symbol(Type t, const std::string& name, const FlowLocation& loc) :
		ASTNode(loc),
		type_(t),
		name_(name),
		owner_(nullptr)
	{}

    friend class SymbolTable;

public:
	Type type() const { return type_; }

	const std::string& name() const { return name_; }
	void setName(const std::string& value) { name_ = value; }
    SymbolTable* owner() const { return owner_; }
};

enum class Lookup {
	Self            = 0x0001, //!< local table only.
	Outer           = 0x0002, //!< outer scope.
	SelfAndOuter    = 0x0003, //!< local scope and any outer scopes
	All             = 0xFFFF,
};

inline bool operator&(Lookup a, Lookup b) {
	return static_cast<unsigned>(a) & static_cast<unsigned>(b);
}

class X0_API SymbolTable {
public:
	typedef std::vector<Symbol*> list_type;
	typedef list_type::iterator iterator;
	typedef list_type::const_iterator const_iterator;

public:
	SymbolTable(SymbolTable* outer, const std::string& name);
	~SymbolTable();

	// nested scoping
	void setOuterTable(SymbolTable* table) { outerTable_ = table; }
	SymbolTable* outerTable() const { return outerTable_; }

	// symbols
	Symbol* appendSymbol(std::unique_ptr<Symbol> symbol);
	void removeSymbol(Symbol* symbol);
	Symbol* symbolAt(size_t i) const;
	size_t symbolCount() const;

	Symbol* lookup(const std::string& name, Lookup lookupMethod) const;

	template<typename T>
	T* lookup(const std::string& name, Lookup lookupMethod) { return dynamic_cast<T*>(lookup(name, lookupMethod)); }

	iterator begin() { return symbols_.begin(); }
	iterator end() { return symbols_.end(); }

    const std::string& name() const { return name_; }

private:
	list_type symbols_;
	SymbolTable* outerTable_;
    std::string name_;
};

class X0_API ScopedSymbol : public Symbol {
protected:
	std::unique_ptr<SymbolTable> scope_;

protected:
	ScopedSymbol(Type t, SymbolTable* outer, const std::string& name, const FlowLocation& loc) :
		Symbol(t, name, loc),
		scope_(new SymbolTable(outer, name))
	{}

	ScopedSymbol(Type t, std::unique_ptr<SymbolTable>&& scope, const std::string& name, const FlowLocation& loc) :
		Symbol(t, name, loc),
		scope_(std::move(scope))
	{}

public:
	SymbolTable* scope() { return scope_.get(); }
	const SymbolTable* scope() const { return scope_.get(); }
	void setScope(std::unique_ptr<SymbolTable>&& table) { scope_ = std::move(table); }
};

class X0_API Variable : public Symbol {
private:
	std::unique_ptr<Expr> initializer_;

public:
	Variable(const std::string& name,
			std::unique_ptr<Expr>&& initializer,
			const FlowLocation& loc) :
		Symbol(Symbol::Variable, name, loc), initializer_(std::move(initializer)) {}

	Expr* initializer() const { return initializer_.get(); }
	void setInitializer(std::unique_ptr<Expr>&& value) { initializer_ = std::move(value); }

	// TODO: should we ref the scope here, for methods like these?
//	bool isLocal() const { return !scope() || scope()->outerTable() != nullptr; }
//	bool isGlobal() const { return !isLocal(); }

	virtual void visit(ASTVisitor& v);
};

class X0_API Callable : public Symbol {
protected:
    FlowVM::Signature signature_;

public:
	Callable(Type t, const FlowVM::Signature& signature, const FlowLocation& loc) :
		Symbol(t, signature.name(), loc),
        signature_(signature)
	{
	}

    bool isHandler() const { return type() == Symbol::Handler || type() == Symbol::BuiltinHandler; }
    bool isBuiltin() const { return type() == Symbol::BuiltinHandler || type() == Symbol::BuiltinFunction; }

    const FlowVM::Signature& signature() const { return signature_; }
};

class X0_API Handler : public Callable {
private:
	std::unique_ptr<SymbolTable> scope_;
	std::unique_ptr<Stmt> body_;

public:
    /** create forward-declared handler. */
	Handler(const std::string& name, const FlowLocation& loc) :
		Callable(Symbol::Handler, FlowVM::Signature(name + "()B"), loc),
		body_(nullptr /*forward declared*/)
	{
	}

    /** create handler. */
	Handler(const std::string& name,
			std::unique_ptr<SymbolTable>&& scope,
			std::unique_ptr<Stmt>&& body,
			const FlowLocation& loc) :
		Callable(Symbol::Handler, FlowVM::Signature(name + "()B"), loc),
		scope_(std::move(scope)),
		body_(std::move(body))
	{
	}

	SymbolTable* scope() { return scope_.get(); }
	const SymbolTable* scope() const { return scope_.get(); }
	Stmt* body() const { return body_.get(); }

	bool isForwardDeclared() const { return body_.get() == nullptr; }

    void implement(std::unique_ptr<SymbolTable>&& table, std::unique_ptr<Stmt>&& body);

	virtual void visit(ASTVisitor& v);
};

class X0_API BuiltinFunction : public Callable {
public:
	BuiltinFunction(const FlowVM::Signature& signature) :
		Callable(Symbol::BuiltinFunction, signature, FlowLocation())
	{
	}

	virtual void visit(ASTVisitor& v);
};

class X0_API BuiltinHandler : public Callable {
public:
	explicit BuiltinHandler(const FlowVM::Signature& signature) :
		Callable(Symbol::BuiltinHandler, signature, FlowLocation())
	{
	}

	virtual void visit(ASTVisitor& v);
};

class X0_API Unit : public ScopedSymbol {
private:
	std::vector<std::pair<std::string, std::string> > imports_;

public:
	Unit() :
		ScopedSymbol(Symbol::Unit, nullptr, "#unit", FlowLocation()),
		imports_()
	{}

	// plugins
	void import(const std::string& moduleName, const std::string& path) {
		imports_.push_back(std::make_pair(moduleName, path));
	}

    const std::vector<std::pair<std::string, std::string>>& imports() const { return imports_; }

	class Handler* findHandler(const std::string& name);

	virtual void visit(ASTVisitor& v);
};
// }}}
// {{{ Expr
class X0_API Expr : public ASTNode {
protected:
	explicit Expr(const FlowLocation& loc) : ASTNode(loc) {}

public:
    virtual FlowType getType() const = 0;
};

class X0_API UnaryExpr : public Expr {
private:
    FlowVM::Opcode operator_;
	std::unique_ptr<Expr> subExpr_;

public:
	UnaryExpr(FlowVM::Opcode op, std::unique_ptr<Expr>&& subExpr, const FlowLocation& loc) :
		Expr(loc), operator_(op), subExpr_(std::move(subExpr))
	{}

    FlowVM::Opcode op() const { return operator_; }
	Expr* subExpr() const { return subExpr_.get(); }

	virtual void visit(ASTVisitor& v);
    virtual FlowType getType() const;
};

class X0_API BinaryExpr : public Expr {
private:
    FlowVM::Opcode operator_;
	std::unique_ptr<Expr> lhs_;
	std::unique_ptr<Expr> rhs_;

public:
	BinaryExpr(FlowVM::Opcode op, std::unique_ptr<Expr>&& lhs, std::unique_ptr<Expr>&& rhs);

    FlowVM::Opcode op() const { return operator_; }
	Expr* leftExpr() const { return lhs_.get(); }
	Expr* rightExpr() const { return rhs_.get(); }

	virtual void visit(ASTVisitor& v);
    virtual FlowType getType() const;
};

template<typename T>
class X0_API LiteralExpr : public Expr {
private:
	T value_;

public:
    explicit LiteralExpr(const T& value) :
        Expr(FlowLocation()), value_(value) {}

    LiteralExpr(const T& value, const FlowLocation& loc) :
        Expr(loc), value_(value) {}

	const T& value() const { return value_; }
	void setValue(const T& value) { value_ = value; }

    virtual FlowType getType() const;

	virtual void visit(ASTVisitor& v) { v.accept(*this); }
};

class X0_API ParamList
{
private:
    bool isNamed_;
    std::vector<std::string> names_;
    std::vector<Expr*> values_;

public:
    ParamList(const ParamList&) = delete;
    ParamList& operator=(const ParamList&) = delete;

    ParamList() : isNamed_(false), names_(), values_() {}
    explicit ParamList(bool named) : isNamed_(named), names_(), values_() {}
    ParamList(ParamList&& v) : isNamed_(v.isNamed_), names_(std::move(v.names_)), values_(std::move(v.values_)) {}
    ParamList& operator=(ParamList&& v);
    ~ParamList();

    void push_back(const std::string& name, std::unique_ptr<Expr>&& arg);
    void push_back(std::unique_ptr<Expr>&& arg);

    bool contains(const std::string& name) const;
    void swap(size_t source, size_t dest);
    void reorder(const FlowVM::NativeCallback* source, std::vector<std::string>* superfluous);
    int find(const std::string& name) const;

    bool isNamed() const { return isNamed_; }

    size_t size() const;
    bool empty() const;
    std::pair<std::string, Expr*> at(size_t offset) const;
    std::pair<std::string, Expr*> operator[](size_t offset) const { return at(offset); }
    const std::vector<std::string>& names() const { return names_; }
    const std::vector<Expr*> values() const { return values_; }

    Expr* back() const { return values_.back(); }

    void dump(const char* title = nullptr);
};

class X0_API FunctionCall : public Expr {
	BuiltinFunction* callee_;
    ParamList args_;

public:
	FunctionCall(const FlowLocation& loc, BuiltinFunction* callee) :
		Expr(loc),
		callee_(callee),
		args_()
	{}

	FunctionCall(const FlowLocation& loc, BuiltinFunction* callee, ParamList&& args) :
		Expr(loc),
		callee_(callee),
		args_(std::move(args))
	{}

	BuiltinFunction* callee() const { return callee_; }
	const ParamList& args() const { return args_; }
	ParamList& args() { return args_; }

	virtual void visit(ASTVisitor& v);
    virtual FlowType getType() const;
};

class X0_API VariableExpr : public Expr
{
private:
	Variable* variable_;

public:
	VariableExpr(Variable* var, const FlowLocation& loc) :
		Expr(loc), variable_(var) {}

	Variable* variable() const { return variable_; }
	void setVariable(Variable* var) { variable_ = var; }

	virtual void visit(ASTVisitor& v);
    virtual FlowType getType() const;
};

class X0_API HandlerRefExpr : public Expr
{
private:
	Handler* handler_;

public:
	HandlerRefExpr(Handler* ref, const FlowLocation& loc) :
		Expr(loc),
		handler_(ref)
	{}

	Handler* handler() const { return handler_; }
	void setHandler(Handler* handler) { handler_ = handler; }

	virtual void visit(ASTVisitor& v);
    virtual FlowType getType() const;
};
// }}}
// {{{ Stmt
class X0_API Stmt : public ASTNode
{
protected:
	explicit Stmt(const FlowLocation& loc) : ASTNode(loc) {}
};

class X0_API ExprStmt : public Stmt
{
private:
	std::unique_ptr<Expr> expression_;

public:
	explicit ExprStmt(std::unique_ptr<Expr>&& expr) :
		Stmt(expr->location()),
		expression_(std::move(expr))
	{}

	Expr *expression() const { return expression_.get(); }
	void setExpression(std::unique_ptr<Expr>&& expr) { expression_ = std::move(expr); }

	virtual void visit(ASTVisitor&);
};

class X0_API CompoundStmt : public Stmt
{
private:
	std::list<std::unique_ptr<Stmt>> statements_;

public:
	explicit CompoundStmt(const FlowLocation& loc) : Stmt(loc) {}

	void push_back(std::unique_ptr<Stmt>&& stmt);

	bool empty() const { return statements_.empty(); }
	size_t count() const { return statements_.size(); }

	std::list<std::unique_ptr<Stmt>>::iterator begin() { return statements_.begin(); }
	std::list<std::unique_ptr<Stmt>>::iterator end() { return statements_.end(); }

	virtual void visit(ASTVisitor&);
};

class X0_API HandlerCall : public Stmt {
protected:
    Callable* callee_;
    ParamList args_;

public:
	HandlerCall(const FlowLocation& loc, Callable* callable) :
		Stmt(loc), callee_(callable), args_()
	{
	}

	HandlerCall(const FlowLocation& loc, Callable* callable, ParamList&& arguments) :
		Stmt(loc), callee_(callable), args_()
	{
		setArgs(std::forward<decltype(arguments)>(arguments));
	}

	bool isHandler() const { return callee_->isHandler(); }

	Callable* callee() const { return callee_; }

	const ParamList& args() const { return args_; }
	ParamList& args() { return args_; }

	bool setArgs(ParamList&& args);

	virtual void visit(ASTVisitor&);
};

class X0_API AssignStmt : public Stmt
{
private:
	Variable* variable_;
	std::unique_ptr<Expr> expr_;

public:
	AssignStmt(Variable* var, std::unique_ptr<Expr> expr, const FlowLocation& loc) :
		Stmt(loc),
		variable_(var),
		expr_(std::move(expr))
	{}

	Variable* variable() const { return variable_; }
	void setVariable(Variable* var) { variable_ = var; }

	Expr* expression() const  { return expr_.get(); }
	void setExpression(std::unique_ptr<Expr> expr) { expr_ = std::move(expr); }

	virtual void visit(ASTVisitor&);
};

class X0_API CondStmt : public Stmt
{
private:
	std::unique_ptr<Expr> cond_;
	std::unique_ptr<Stmt> thenStmt_;
	std::unique_ptr<Stmt> elseStmt_;

public:
	CondStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> thenStmt,
			std::unique_ptr<Stmt> elseStmt, const FlowLocation& loc) :
		Stmt(loc),
		cond_(std::move(cond)),
		thenStmt_(std::move(thenStmt)),
		elseStmt_(std::move(elseStmt))
	{}

	Expr* condition() const { return cond_.get(); }
	void setCondition(std::unique_ptr<Expr> cond) { cond_ = std::move(cond); }

	Stmt* thenStmt() const { return thenStmt_.get(); }
	void setThenStmt(std::unique_ptr<Stmt> stmt) { thenStmt_ = std::move(stmt); }

	Stmt* elseStmt() const { return elseStmt_.get(); }
	void setElseStmt(std::unique_ptr<Stmt> stmt) { elseStmt_ = std::move(stmt); }

	virtual void visit(ASTVisitor&);
};

typedef std::pair<std::unique_ptr<Expr>, std::unique_ptr<Stmt>> MatchCase;

class X0_API MatchStmt : public Stmt
{
public:
    typedef std::list<MatchCase> CaseList;

    MatchStmt(const FlowLocation& loc, std::unique_ptr<Expr>&& cond, FlowToken op, std::list<MatchCase>&& cases, std::unique_ptr<Stmt>&& elseStmt);
    MatchStmt(MatchStmt&& other);
    MatchStmt& operator=(MatchStmt&& other);
    ~MatchStmt();

    Expr* condition() { return cond_.get(); }
    FlowToken op() const { return op_; }
    CaseList& cases() { return cases_; }

	Stmt* elseStmt() const { return elseStmt_.get(); }
	void setElseStmt(std::unique_ptr<Stmt> stmt) { elseStmt_ = std::move(stmt); }

	virtual void visit(ASTVisitor&);

private:
    std::unique_ptr<Expr> cond_;
    FlowToken op_;
    CaseList cases_;
    std::unique_ptr<Stmt> elseStmt_;
};
// }}}

} // namespace x0
