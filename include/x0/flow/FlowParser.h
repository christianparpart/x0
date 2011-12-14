/* <flow/FlowParser.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_flow_parser_h
#define sw_flow_parser_h (1)

#include <map>
#include <list>
#include <string>
#include <functional>

#include <x0/flow/FlowToken.h>
#include <x0/flow/Flow.h> // SymbolTable
#include <istream>

namespace x0 {

class FlowBackend;

class FlowParser
{
private:
	FlowLexer* lexer_;
	std::list<SymbolTable*> scopeStack_;
	std::function<void(const std::string&)> errorHandler_;
	FlowBackend* backend_;

public:
	explicit FlowParser(FlowBackend* b);
	~FlowParser();

	bool initialize(std::istream *stream, const std::string& name = std::string());

	Unit *parse();

	std::string dump() const;

	void setErrorHandler(std::function<void(const std::string&)>&& callback);

private:
	void reportUnexpectedToken();
	void reportError(const std::string& message);
	template<typename... Args> void reportError(const std::string& fmt, Args&& ...);

	size_t line() const;
	FlowToken token() const;
	FlowToken nextToken() const;
	bool eof() const;
	bool skip(FlowToken);
	bool consume(FlowToken);
	bool consumeUntil(FlowToken);

	template<typename A1, typename... Args> bool consumeOne(A1 token, Args... tokens);
	template<typename A1> bool testTokens(A1 token) const;
	template<typename A1, typename... Args> bool testTokens(A1 token, Args... tokens) const;

	std::string stringValue() const;
	double numberValue() const;
	bool booleanValue() const;

	SymbolTable *scope();
	SymbolTable *enter(SymbolTable *scope);
	SymbolTable *leave();

	template<typename T, typename... Args> T *lookupOrCreate(const std::string& name, Args&&... args);

	template<typename T, typename... Args> T *createSymbol(const std::string& name, Args&&... args)
	{
		T *symbol = new T(args...);
		scope()->appendSymbol(symbol);
		return symbol;
	}

private:
	Unit *unit();

	// decls
	Symbol *decl();
	bool importDecl(Unit *unit);
	bool importOne(std::list<std::string>& names);
	Variable *varDecl();
	Function *handlerDecl();
	Function *externDecl();

	// expressions
	Expr *expr();
	Expr *logicExpr();
	Expr *negExpr();
	Expr *relExpr();
	Expr *addExpr();
	Expr *mulExpr();
	Expr *powExpr();
	Expr *primaryExpr();
	Expr *subExpr();
	Expr *literalExpr();
	Expr *hashExpr();
	Expr *symbolExpr();
	Expr *callExpr();

	ListExpr *exprList();

	// statements
	Stmt *stmt();
	Stmt *ifStmt();
	Stmt *callStmt();
	Stmt *compoundStmt();
	Stmt *postscriptStmt(Stmt *baseStmt);
	Stmt *postscriptIfStmt(Stmt *baseStmt);

	// location service
	SourceLocation location() { return lexer_->location(); }
	FilePos end() { return lexer_->lastLocation().end; }
};

// {{{ inlines
inline FlowToken FlowParser::token() const
{
	return lexer_->token();
}

inline size_t FlowParser::line() const
{
	return lexer_->line();
}

inline bool FlowParser::eof() const
{
	return lexer_->eof();
}

inline std::string FlowParser::stringValue() const
{
	return lexer_->stringValue();
}

inline double FlowParser::numberValue() const
{
	return lexer_->numberValue();
}

inline bool FlowParser::booleanValue() const
{
	return lexer_->booleanValue();
}

inline SymbolTable *FlowParser::scope()
{
	return scopeStack_.back();
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

template<typename A1> inline bool FlowParser::testTokens(A1 a1) const
{
	return token() == a1;
}

template<typename A1, typename... Args> inline bool FlowParser::testTokens(A1 a1, Args... tokens) const
{
	if (token() == a1)
		return true;

	return testTokens(tokens...);
}

template<typename... Args>
inline void FlowParser::reportError(const std::string& fmt, Args&&... args)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), fmt.c_str(), args...);
	reportError(buf);
}

template<typename T, typename... Args>
T *FlowParser::lookupOrCreate(const std::string& name, Args&&... args)
{
	if (T *result = static_cast<T *>(scope()->lookup(name, Lookup::All)))
		return result;

	// create symbol in global-scope
	T *result = new T(name, args...);
	scopeStack_.front()->appendSymbol(result);
	return result;
}

// }}}

} // namespace x0

#endif
