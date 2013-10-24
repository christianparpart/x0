/* <flow/FlowParser.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_flow_parser_h
#define sw_flow_parser_h (1)

#include <list>
#include <string>
#include <functional>

#include <x0/Api.h>
#include <x0/Utility.h>
#include <x0/flow2/FlowToken.h>
#include <x0/flow2/FlowLexer.h>
#include <x0/flow2/AST.h> // SymbolTable

namespace x0 {

class FlowBackend;
class FlowLexer;

class X0_API FlowParser {
	std::unique_ptr<FlowLexer> lexer_;
	std::list<SymbolTable*> scopeStack_;
	FlowBackend* backend_;

public:
	std::function<void(const std::string&)> errorHandler;
	std::function<bool(const std::string&, const std::string&)> importHandler;

	explicit FlowParser(FlowBackend* backend);
	~FlowParser();

	bool open(const std::string& filename);

	std::unique_ptr<Unit> parse();

	FlowBackend* backend() const { return backend_; }

private:
	class Scope;

	// error handling
	void reportUnexpectedToken();
	void reportError(const std::string& message);
	template<typename... Args> void reportError(const std::string& fmt, Args&& ...);

	// lexing
	FlowToken token() const { return lexer_->token(); }
	const FlowLocation& location() { return lexer_->location(); }
	const FilePos& end() const { return lexer_->location().end; }
	FlowToken nextToken() const;
	bool eof() const { return lexer_->eof(); }
	bool consume(FlowToken);
	bool consumeIf(FlowToken);
	bool consumeUntil(FlowToken);

	template<typename A1, typename... Args> bool consumeOne(A1 token, Args... tokens);
	template<typename A1> bool testTokens(A1 token) const;
	template<typename A1, typename... Args> bool testTokens(A1 token, Args... tokens) const;

	std::string stringValue() const { return lexer_->stringValue(); }
	double numberValue() const { return lexer_->numberValue(); }
	bool booleanValue() const { return lexer_->numberValue(); }

	// scoping
	SymbolTable *scope() { return scopeStack_.front(); }
	SymbolTable *enter(SymbolTable *scope);
	std::unique_ptr<SymbolTable> enterScope() { return std::unique_ptr<SymbolTable>(enter(new SymbolTable(scope()))); }
	SymbolTable *leaveScope() { return leave(); }
	SymbolTable *leave();

	// symbol mgnt
	template<typename T> T* lookup(const std::string& name)
	{
		if (T* result = static_cast<T*>(scope()->lookup(name, Lookup::All)))
			return result;

		return nullptr;
	}

	template<typename T, typename... Args> T* createSymbol(const std::string& name, Args&&... args)
	{
		return static_cast<T*>(scope()->appendSymbol(std::make_unique<T>(name, std::forward<Args>(args)...)));
	}

	template<typename T, typename... Args> T* lookupOrCreate(const std::string& name, Args&&... args)
	{
		if (T* result = static_cast<T*>(scope()->lookup(name, Lookup::All)))
			return result;

		// create symbol in global-scope
		T* result = new T(name, args...);
		scopeStack_.front()->appendSymbol(result);
		return result;
	}

	// syntax: decls
	std::unique_ptr<Unit> unit();
	bool importDecl(Unit *unit);
	bool importOne(std::list<std::string>& names);
	std::unique_ptr<Symbol> decl();
	std::unique_ptr<Variable> varDecl();
	std::unique_ptr<Handler> handlerDecl();

	// syntax: expressions
	std::unique_ptr<Expr> expr();
	std::unique_ptr<Expr> rhsExpr(std::unique_ptr<Expr> lhs, int precedence);
	std::unique_ptr<Expr> powExpr();
	std::unique_ptr<Expr> primaryExpr();
	std::unique_ptr<Expr> interpolatedStr();
	std::unique_ptr<Expr> castExpr();
	std::unique_ptr<ListExpr> listExpr();

	// syntax: statements
	std::unique_ptr<Stmt> stmt();
	std::unique_ptr<Stmt> ifStmt();
	std::unique_ptr<Stmt> compoundStmt();
	std::unique_ptr<Stmt> callStmt();
	std::unique_ptr<Stmt> postscriptStmt(std::unique_ptr<Stmt> baseStmt);
};

// {{{ inlines
template<typename... Args>
inline void FlowParser::reportError(const std::string& fmt, Args&&... args)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), fmt.c_str(), args...);
	reportError(buf);
}

template<typename A1, typename... Args>
inline bool FlowParser::consumeOne(A1 a1, Args... tokens)
{
	if (!testTokens(a1, tokens...))
	{
		reportUnexpectedToken();
		return false;
	}

	nextToken();
	return true;
}

template<typename A1>
inline bool FlowParser::testTokens(A1 a1) const
{
	return token() == a1;
}

template<typename A1, typename... Args>
inline bool FlowParser::testTokens(A1 a1, Args... tokens) const
{
	if (token() == a1)
		return true;

	return testTokens(tokens...);
}

// }}}

} // namespace x0

#endif
