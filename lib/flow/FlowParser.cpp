/* <flow/FlowParser.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/flow/FlowParser.h>
#include <x0/flow/FlowLexer.h>
#include <x0/flow/FlowBackend.h>
#include <x0/flow/Flow.h>
#include <x0/Defines.h>
#include <x0/RegExp.h>

namespace x0 {

//#define FLOW_DEBUG_PARSER 1

#if defined(FLOW_DEBUG_PARSER)
// {{{ trace
static size_t fnd = 0;
struct fntrace {
	std::string msg_;

	fntrace(const char *msg) : msg_(msg)
	{
		++fnd;

		for (size_t i = 0; i < fnd; ++i)
			printf("  ");

		printf("-> %s\n", msg_.c_str());
	}

	~fntrace() {
		for (size_t i = 0; i < fnd; ++i)
			printf("  ");

		--fnd;

		printf("<- %s\n", msg_.c_str());
	}
};
// }}}
#	define FNTRACE() fntrace _(__PRETTY_FUNCTION__)
#	define TRACE(msg...) DEBUG("FlowParser: " msg)
#else
#	define FNTRACE() /*!*/
#	define TRACE(msg...) /*!*/
#endif

inline Operator makeUnaryOperator(FlowToken token) // {{{
{
	switch (token) {
		case FlowToken::Plus:
			return Operator::UnaryPlus;
		case FlowToken::Minus:
			return Operator::UnaryMinus;
		case FlowToken::Not:
			return Operator::Not;
		default:
			return Operator::Undefined;
	}
} // }}}

inline Operator makeOperator(FlowToken token) // {{{
{
	switch (token) {
		case FlowToken::Equal:
			return Operator::Equal;
		case FlowToken::UnEqual:
			return Operator::UnEqual;
		case FlowToken::Less:
			return Operator::Less;
		case FlowToken::Greater:
			return Operator::Greater;
		case FlowToken::LessOrEqual:
			return Operator::LessOrEqual;
		case FlowToken::GreaterOrEqual:
			return Operator::GreaterOrEqual;
		case FlowToken::PrefixMatch:
			return Operator::PrefixMatch;
		case FlowToken::SuffixMatch:
			return Operator::SuffixMatch;
		case FlowToken::RegexMatch:
			return Operator::RegexMatch;
		case FlowToken::Plus:
			return Operator::Plus;
		case FlowToken::Minus:
			return Operator::Minus;
		case FlowToken::Mul:
			return Operator::Mul;
		case FlowToken::Div:
			return Operator::Div;
		case FlowToken::Mod:
			return Operator::Mod;
		case FlowToken::Shl:
			return Operator::Shl;
		case FlowToken::Shr:
			return Operator::Shr;
		case FlowToken::Pow:
			return Operator::Pow;
		case FlowToken::Not:
			return Operator::Not;
		case FlowToken::And:
			return Operator::And;
		case FlowToken::Or:
			return Operator::Or;
		case FlowToken::Xor:
			return Operator::Xor;
		case FlowToken::In:
			return Operator::In;
		default:
			printf("makeOperator: unsupported operator %d\n", (int)token);
			return Operator::Undefined;
	}
} // }}}

FlowParser::FlowParser(FlowBackend* b) :
	lexer_(NULL),
	scopeStack_(),
	errorHandler_(),
	backend_(b)
{
	lexer_ = new FlowLexer();
}

FlowParser::~FlowParser()
{
	delete lexer_;
}

std::string FlowParser::dump() const
{
	return lexer_->dump();
}

void FlowParser::setErrorHandler(std::function<void(const std::string&)>&& callback)
{
	errorHandler_ = callback;
}

void FlowParser::reportError(const std::string& message)
{
	if (errorHandler_)
	{
		char buf[1024];
		snprintf(buf, sizeof(buf), "[%04ld:%02ld] %s",
				lexer_->line(), lexer_->column(), message.c_str());

		errorHandler_(buf);
	}
}

bool FlowParser::initialize(std::istream *stream, const std::string& name)
{
	return lexer_->initialize(stream, name);
}

FlowToken FlowParser::nextToken() const
{
#if defined(FLOW_DEBUG_PARSER)
	FlowToken t = lexer_->nextToken();
	lexer_->dump();
	return t;
#else
	return lexer_->nextToken();
#endif
}

void FlowParser::reportUnexpectedToken()
{
	reportError("Unexpected token '%s'", lexer_->tokenString().c_str());
}

bool FlowParser::consume(FlowToken value)
{
	if (token() != value)
	{
		reportError("Unexpected token '%s' (expected: '%s')",
				lexer_->tokenString().c_str(),
				lexer_->tokenToString(value).c_str());
		return false;
	}
	nextToken();
	return true;
}

bool FlowParser::skip(FlowToken value)
{
	if (token() != value)
		return false;
	
	nextToken();
	return true;
}

bool FlowParser::consumeUntil(FlowToken value)
{
	for (;;)
	{
		if (token() == value) {
			nextToken();
			return true;
		}

		if (token() == FlowToken::Eof)
			return false;

		nextToken();
	}
}

SymbolTable *FlowParser::enter(SymbolTable *scope)
{
	scopeStack_.push_back(scope);
	return scope;
}

SymbolTable *FlowParser::leave()
{
	SymbolTable *popped = scopeStack_.back();
	scopeStack_.pop_back();
	return popped;
}

Unit *FlowParser::parse()
{
	FNTRACE();
	return unit();
}

// {{{ declarations
Unit *FlowParser::unit()
{
	FNTRACE();
	Unit *unit = new Unit();

	enter(&unit->members());

	while (token() == FlowToken::Import)
		if (!importDecl(unit))
			goto err;

	while (Symbol *sym = decl()) {
		TRACE("unit: parsed symbol: %s", sym->name().c_str());
		unit->insert(sym);
	}

	leave();

	if (token() == FlowToken::Eof)
		return unit;

err:
	delete unit;
	return NULL;
}

Symbol *FlowParser::decl()
{
	FNTRACE();

	switch (token())
	{
		case FlowToken::Var:
			return varDecl();
		case FlowToken::Handler:
			return handlerDecl();
		case FlowToken::Extern:
			return externDecl();
		default:
			return NULL;
	}
}

bool FlowParser::importDecl(Unit *unit)
{
	FNTRACE();

	// 'import' NAME_OR_NAMELIST ['from' PATH] ';'
	nextToken(); // skip 'import'

	std::list<std::string> names;
	if (!importOne(names)) {
		consumeUntil(FlowToken::Semicolon);
		return false;
	}

	while (token() == FlowToken::Comma)
	{
		nextToken();
		if (!importOne(names)) {
			consumeUntil(FlowToken::Semicolon);
			return false;
		}
	}

	std::string path;
	if (skip(FlowToken::From))
	{
		path = stringValue();

		if (!consumeOne(FlowToken::String, FlowToken::RawString)) {
			consumeUntil(FlowToken::Semicolon);
			return false;
		}

		if (!path.empty() && path[0] != '/') {
			std::string base(lexer_->location().fileName);

			size_t r = base.rfind('/');
			if (r != std::string::npos)
				++r;

			base = base.substr(0, r);
			path = base + path;
		}
	}

	for (auto i = names.begin(), e = names.end(); i != e; ++i)
		unit->import(*i, path);

	skip(FlowToken::Semicolon);
	return true;
}

bool FlowParser::importOne(std::list<std::string>& names)
{
	switch (token())
	{
		case FlowToken::Ident:
		case FlowToken::String:
		case FlowToken::RawString:
			names.push_back(stringValue());
			nextToken();
			break;
		case FlowToken::RndOpen:
			nextToken();
			if (!importOne(names))
				return false;

			while (token() == FlowToken::Comma)
			{
				nextToken(); // skip comma
				if (!importOne(names))
					return false;
			}

			if (!consume(FlowToken::RndClose))
				return false;
			break;
		default:
			reportError("Syntax error in import declaration.");
			return false;
	}
	return true;
}

Variable *FlowParser::varDecl()
{
	FNTRACE();
	SourceLocation sloc(location());

	if (!consume(FlowToken::Var))
		return NULL;

	if (!consume(FlowToken::Ident))
		return NULL;

	std::string name = stringValue();

	if (!consume(FlowToken::Assign))
		return NULL;

	Expr *value = expr();
	if (!value)
		return NULL;

	sloc.update(end());
	skip(FlowToken::Semicolon);

	return new Variable(scope(), name, value, sloc);
}

Function *FlowParser::handlerDecl()
{
	// handlerDecl ::= 'handler' (';' | [do] stmt)

	FNTRACE();
	SourceLocation sloc(location());
	nextToken(); // 'handler'

	consume(FlowToken::Ident);
	std::string name = stringValue();

	if (skip(FlowToken::Semicolon)) { // forward-declaration
		sloc.update(end());
		return new Function(NULL, name, NULL, true, sloc);
	}

	skip(FlowToken::Do);

	SymbolTable *st = new SymbolTable(scope());
	enter(st);
	Stmt *body = stmt();
	leave();

	if (!body) {
		delete st;
		return NULL;
	}

	sloc.update(end());
	Function *f = static_cast<Function *>(scope()->lookup(name, Lookup::Self));
	if (!f)
		return new Function(st, name, body, true, sloc);

	// ensure `f` is forward-declared.
	if (f->body()) {
		reportError("Redeclaring handler '%s'", f->name().c_str());
		delete body;
		delete st;
		return nullptr;
	}
	// ensure `f` is forward-declared as handler (and not non-handler)
	if (!f->isHandler()) {
		reportError("Redeclaring function '%s' as handler is invalid.", f->name().c_str());
		delete body;
		delete st;
		return nullptr;
	}
	f->setScope(st);
	f->setBody(body);

	// as `f` was once a forward decl we have to remove it from its scope before leaving.
	scope()->removeSymbol(f);

	return f;
}

Function *FlowParser::externDecl()
{
	// externDecl ::= 'extern' TYPE NAME '(' [TYPE (',' TYPE)*] ')' ';'
	FNTRACE();
	SourceLocation sloc(location());
	nextToken(); // 'extern'

	FlowToken ty = token(); // return type
	nextToken();

	// function name
	std::string name = stringValue();
	if (!consume(FlowToken::Ident))
		return NULL;

	sloc.update(end());
	Function *fn = new Function(new SymbolTable(scope()), name, NULL, false, sloc);

	fn->setReturnType(ty);

	if (skip(FlowToken::RndOpen))
	{
		if (FlowTokenTraits::isType(token()))
		{
			// add first arg type
			fn->argTypes().push_back(token());
			nextToken();

			while (token() == FlowToken::Comma)
			{
				nextToken(); // consume comma

				if (FlowTokenTraits::isType(token()))
					fn->argTypes().push_back(token());
				else if (token() == FlowToken::Ellipsis)
				{
					nextToken();
					fn->setIsVarArg(true);
					break;
				}
				else
				{
					reportError("Unknown function parameter type: %s", lexer_->tokenString().c_str());
					delete fn;
					return NULL;
				}

				nextToken(); // consume type
			}
		}

		if (!skip(FlowToken::RndClose)) {
			delete fn;
			return NULL;
		}
	}
	consume(FlowToken::Semicolon);

	return fn;
}
// }}}

// {{{ expressions
Expr *FlowParser::expr() // logicExpr
{
	FNTRACE();
	return logicExpr();
}

Expr *FlowParser::logicExpr()
{
	FNTRACE();
	SourceLocation sloc(location());

	Expr *left = negExpr();
	if (!left)
		return NULL;

	for (;;) {
		switch (token()) {
			case FlowToken::And:
			case FlowToken::Or:
			case FlowToken::Xor:
			{
				Operator op = makeOperator(token());
				nextToken();

				Expr *right = negExpr();
				if (!right) {
					delete left;
					return NULL;
				}

				sloc.update(end());
				left = new BinaryExpr(op, left, right, sloc);
				break;
			}
			default:
				return left;
		}
	}
}

Expr *FlowParser::negExpr()
{
	// negExpr ::= ('!')* relExpr
	FNTRACE();
	SourceLocation sloc(location());

	bool negate = false;

	while (skip(FlowToken::Not))
		negate = !negate;

	Expr *e = relExpr();

	sloc.update(end());

	if (e && negate)
		e = new UnaryExpr(Operator::Not, e, sloc);

	return e;
}

Expr *FlowParser::relExpr() // addExpr ((== != <= >= < > =^ =$ =~ 'in') addExpr)*
{
	FNTRACE();
	SourceLocation sloc(location());
	Expr *left = addExpr();

	for (;;)
	{
		switch (token())
		{
			case FlowToken::Equal:
			case FlowToken::UnEqual:
			case FlowToken::LessOrEqual:
			case FlowToken::GreaterOrEqual:
			case FlowToken::Less:
			case FlowToken::Greater:
			case FlowToken::PrefixMatch:
			case FlowToken::SuffixMatch:
			case FlowToken::RegexMatch:
			case FlowToken::In:
			{
				Operator op = makeOperator(token());
				nextToken();
				Expr *right = addExpr();
				if (!right) {
					delete left;
					return NULL;
				}
				left = new BinaryExpr(op, left, right, sloc.update(end()));
			}
			default:
				return left;
		}
	}
}

Expr *FlowParser::addExpr() // mulExpr (('+' | '-') mulExpr)*
{
	FNTRACE();
	SourceLocation sloc(location());
	Expr *left = mulExpr();
	if (!left)
		return NULL;

	for (;;) {
		switch (token()) {
			case FlowToken::Plus:
			case FlowToken::Minus:
			{
				Operator op = makeOperator(token());
				nextToken();

				Expr *right = mulExpr();
				if (!right) {
					delete left;
					return NULL;
				}

				left = new BinaryExpr(op, left, right, sloc.update(end()));
				break;
			}
			default:
				return left;
		}
	}
}

Expr *FlowParser::mulExpr()
{
	// powExpr (('*' | '/' | shl | shr) powExpr)*

	FNTRACE();
	SourceLocation sloc(location());
	Expr *left = powExpr();
	if (!left)
		return NULL;

	for (;;) {
		switch (token()) {
			case FlowToken::Mul:
			case FlowToken::Div:
			case FlowToken::Shl:
			case FlowToken::Shr:
			{
				Operator op = makeOperator(token());
				nextToken();

				Expr *right = powExpr();
				if (!right) {
					delete left;
					return NULL;
				}
				left = new BinaryExpr(op, left, right, sloc.update(end()));
				break;
			}
			default:
				return left;
		}
	}
}

Expr *FlowParser::powExpr()
{
	// primaryExpr ('**' primaryExpr)*
	FNTRACE();
	SourceLocation sloc(location());
	Expr *left = primaryExpr();
	if (!left)
		return NULL;

	for (;;) {
		switch (token()) {
			case FlowToken::Pow:
			{
				Operator op = makeOperator(token());
				nextToken();

				Expr *right = primaryExpr();
				if (!right) {
					delete left;
					return NULL;
				}
				left = new BinaryExpr(op, left, right, sloc.update(end()));
				break;
			}
			default:
				return left;
		}
	}
}

Expr *FlowParser::primaryExpr()
{
	// primaryExpr ::= subExpr UNIT?
	FNTRACE();
	SourceLocation sloc(location());

	Expr *left = subExpr();

	if (token() == FlowToken::Ident)
	{
		static struct {
			const char *ident;
			long long nominator;
			long long denominator;
		} units[] = {
			{ "byte", 1, 1 },
			{ "kbyte", 1024llu, 1 },
			{ "mbyte", 1024llu * 1024, 1 },
			{ "gbyte", 1024llu * 1024 * 1024, 1 },
			{ "tbyte", 1024llu * 1024 * 1024 * 1024, 1 },
			{ "bit", 1, 8 },
			{ "kbit", 1024llu, 8 },
			{ "mbit", 1024llu * 1024, 8 },
			{ "gbit", 1024llu * 1024 * 1024, 8 },
			{ "tbit", 1024llu * 1024 * 1024 * 1024, 8 },
			{ "sec", 1, 1 },
			{ "min", 60llu, 1 },
			{ "hour", 60llu * 60, 1 },
			{ "day", 60llu * 60 * 24, 1 },
			{ "week", 60llu * 60 * 24 * 7, 1 },
			{ "month", 60llu * 60 * 24 * 30, 1 },
			{ "year", 60llu * 60 * 24 * 365, 1 },
			{ NULL, 1, 1 }
		};

		std::string sv(stringValue());

		for (size_t i = 0; units[i].ident; ++i)
		{
			if (sv == units[i].ident
				|| (sv[sv.size() - 1] == 's' && sv.substr(0, sv.size() - 1) == units[i].ident))
			{
				nextToken();
				Expr *nominator = new NumberExpr(units[i].nominator, SourceLocation());
				Expr *denominator = new NumberExpr(units[i].denominator, SourceLocation());
				left = new BinaryExpr(
					Operator::Div,
					new BinaryExpr(Operator::Mul, left, nominator, sloc.update(end())),
					denominator, SourceLocation());
				break;
			}
		}
	}

	return left;
}

Expr *FlowParser::subExpr()
{
	// subExpr ::= literalExpr ['=>' expr]
	//           | symbolExpr
	//			 | '(' expr ')'
	//			 | '{' stmt '}'

	FNTRACE();
	SourceLocation sloc(location());

	switch (token())
	{
		case FlowToken::Begin: // lambda-like inline function ref
		{
			SymbolTable *st = new SymbolTable(scope());
			enter(st);
			Stmt *body = compoundStmt();
			leave();

			char name[64];
			static unsigned long i = 0;
			snprintf(name, sizeof(name), "__lambda_%ld", ++i);

			Function *f = new Function(st, name, body, true, body->sourceLocation());
			return new FunctionRefExpr(f, body->sourceLocation());
		}
		case FlowToken::RndOpen:
		{
			nextToken();
			if (token() != FlowToken::RndClose)
			{
				Expr *e = expr();

				if (token() == FlowToken::Comma) {
					ListExpr *le = new ListExpr(sloc);
					le->push_back(e);
					e = le;

					do
					{
						nextToken(); // skip comma
						Expr *elem = expr();
						if (elem)
							le->push_back(elem);
						else {
							delete e;
							return NULL;
						}
					}
					while (token() == FlowToken::Comma);
				}

				if (!consume(FlowToken::RndClose)) {
					delete e;
					return NULL;
				}

				e->setSourceLocation(sloc.update(end()));
				return e;
			}
			else
			{
				if (!consume(FlowToken::RndClose))
					return NULL;

				return new ListExpr(sloc.update(end()));
			}
		}
		case FlowToken::BrOpen: // [ expr [',' expr]* ]
			return hashExpr();
		case FlowToken::Ident:
			return symbolExpr();
		default:
		{
			Expr *left = literalExpr();

			if (!left || !skip(FlowToken::KeyAssign))
				return left;

			Expr *right = expr();
			if (!right) {
				delete left;
				return NULL;
			}

			ListExpr *e = new ListExpr(sloc.update(end()));
			e->push_back(left);
			e->push_back(right);
			return e;
		}
	}
}

Expr *FlowParser::literalExpr()
{
	// LiteralExpr ::= number | string | bool | regexp | ip

	FNTRACE();
	SourceLocation sloc(location());

	switch (token()) {
		case FlowToken::RegExp: {
			Expr *e = new RegExpExpr(RegExp(stringValue()), sloc.update(end()));
			nextToken();
			return e;
		}
		case FlowToken::String:
		case FlowToken::RawString: {
			Expr *e = new StringExpr(stringValue(), sloc.update(end()));
			nextToken();
			return e;
		}
		case FlowToken::Boolean: {
			Expr *e = new BoolExpr(booleanValue(), sloc.update(end()));
			nextToken();
			return e;
		}
		case FlowToken::Number:
		{
			Expr *e = new NumberExpr(numberValue(), sloc.update(end()));
			nextToken();
			return e;
		}
		case FlowToken::IP:
		{
			Expr *e = new IPAddressExpr(lexer_->ipValue(), sloc.update(end()));
			nextToken();
			return e;
		}
		default:
			reportError("Unexpected token: %s", lexer_->tokenString().c_str());
			return NULL;
	}
}

Expr *FlowParser::symbolExpr()
{
	// symbolExpr ::= variableExpr | functionCallExpr
	//            ::= NAME
	//              | NAME '(' exprList ')'

	FNTRACE();
	SourceLocation sloc(location());

	std::string name = stringValue();
	if (!consume(FlowToken::Ident))
		return NULL;

	if (token() == FlowToken::RndOpen)
	{
		// function call expression
		nextToken();
		ListExpr *args = NULL;
		if (token() != FlowToken::RndClose) {
			args = exprList();
			if (!args || !consume(FlowToken::RndClose)) {
				delete args;
				return NULL;
			}
		} else if (!consume(FlowToken::RndClose)) {
			return NULL;
		}
		sloc.update(end());
		return new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Method, sloc);
	}
	else
	{
		// variable / handler ref expression
		sloc.update(end());

		Symbol *symbol = scope()->lookup(name, Lookup::All);
		if (symbol) {
			switch (symbol->type()) {
			case Symbol::VARIABLE:
				return new VariableExpr(static_cast<Variable*>(symbol), sloc);
			case Symbol::FUNCTION:
				return new FunctionRefExpr(static_cast<Function*>(symbol), sloc);
			default:
				reportError("Invalid reference to symbol '%s'", name.c_str());
				return NULL;
			}
		} else {
			if (backend_->isProperty(name)) {
				//printf("var symbol referenced: '%s'\n", name.c_str());
				Variable *v = new Variable(name, sloc);
				scopeStack_.front()->appendSymbol(v);
				return new VariableExpr(v, sloc);
			} else {
				//printf("fnref symbol referenced: '%s'\n", name.c_str());
				Function *f = new Function(name, true);
				scopeStack_.front()->appendSymbol(f); // register forward to global-scope
				return new FunctionRefExpr(f, sloc);
			}
		}
	}
}

ListExpr *FlowParser::exprList() // expr (',' expr)
{
	FNTRACE();
	SourceLocation sloc(location());

	ListExpr *list = new ListExpr(sloc);

	Expr *e = expr();
	if (!e)
		return NULL;

	list->push_back(e);

	while (token() == FlowToken::Comma)
	{
		nextToken();
		e = expr();
		if (!e)
		{
			delete list;
			return NULL;
		}
		list->push_back(e);
	}

	list->sourceLocation().update(end());
	return list;
}

Expr *FlowParser::hashExpr()
{
	// '[' (expr (',' expr)*)? ']'

	FNTRACE();
	SourceLocation sloc(location());

	if (!consume(FlowToken::BrOpen))
		return NULL;

	if (token() == FlowToken::BrClose)
	{
		nextToken();
		return new ListExpr(sloc.update(end()));
	}

	ListExpr *e = exprList();
	if (!consume(FlowToken::BrClose)) {
		delete e;
		return NULL;
	}
	e->setSourceLocation(sloc.update(end()));
	return e;
}

Expr *FlowParser::callExpr()
{
	// callExpr ::= NAME '(' exprList? ')'
	// where NAME *must* be a function that is not a handler.

	FNTRACE();
	SourceLocation sloc(location());

	std::string name = stringValue();
	if (!consume(FlowToken::Ident))
		return NULL;

	if (token() != FlowToken::RndOpen) // reference to a handler
	{
		sloc.update(end());
		Function *f = lookupOrCreate<Function>(name, true);
		if (!f->isHandler()) {
			reportError("Symbol '%s' must be a handler", name.c_str());
			return NULL;
		}
		return new FunctionRefExpr(f, sloc);
	}
	nextToken(); // RndOpen

	if (!consume(FlowToken::RndOpen))
		return NULL;

	ListExpr *args = exprList();
	if (!args)
		return NULL;

	if (!consume(FlowToken::RndClose)) {
		delete args;
		return NULL;
	}

	sloc.update(end());
	return new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Method, sloc);
}
// }}}

//{{{ statements
Stmt *FlowParser::stmt()
{
	FNTRACE();

	switch (token()) {
		case FlowToken::If:
			return ifStmt();
		case FlowToken::Begin:
			return compoundStmt();
		case FlowToken::Ident:
			return callStmt();
		case FlowToken::Semicolon:
		{
			SourceLocation sloc(location());
			nextToken();
			return new CompoundStmt(sloc.update(end()));
		}
		default:
			reportError("Unexpected token '%s'. Expected a statement instead.", lexer_->tokenToString(token()).c_str());
			return NULL;
	}
}

Stmt *FlowParser::ifStmt()
{
	FNTRACE();
	SourceLocation sloc(location());
	consume(FlowToken::If);

	Expr *cond = expr();

	skip(FlowToken::Then);

	Stmt *thenStmt = stmt();
	if (!thenStmt) {
		delete cond;
		return NULL;
	}

	Stmt *elseStmt = NULL;

	if (skip(FlowToken::Else))
	{
		elseStmt = stmt();
		if (!elseStmt)
		{
			delete cond;
			return NULL;
		}
	}

	return new CondStmt(cond, thenStmt, elseStmt, sloc.update(end()));
}

Stmt *FlowParser::callStmt()
{
	// callStmt ::= NAME ['(' exprList ')' | exprList] (';' | LF)
	// NAME may be a variable-, function- or handler-name.

	FNTRACE();
	SourceLocation sloc(location());
	//sloc.dump(token().c_str());

	std::string name = stringValue();
	if (!consume(FlowToken::Ident))
		return NULL;

	switch (token())
	{
		case FlowToken::If:
			return postscriptStmt(new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), NULL/*args*/, CallExpr::Undefined, sloc), sloc));
		case FlowToken::Semicolon:
			// must be a function/handler

			nextToken();
			sloc.update(end());
			return new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), NULL/*args*/, CallExpr::Undefined, sloc), sloc);
		case FlowToken::Assign:
		{
			// must be a writable variable.
			nextToken(); // skip '='
			Expr *e = expr();
			if (!e)
				return NULL;

			sloc.update(end());

			ListExpr *args = new ListExpr(e->sourceLocation());
			args->push_back(e);

			return new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Assignment, sloc), sloc);
		}
		case FlowToken::RndOpen:
		{
			// must be a function/handler

			nextToken();
			ListExpr *args = NULL;
			if (token() != FlowToken::RndClose) {
				args = exprList();
				if (!args) {
					delete args;
					return NULL;
				}
			}
			if (!consume(FlowToken::RndClose)) {
				delete args;
				return NULL;
			}
			sloc.update(end());
			Stmt *stmt = new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Method, sloc), sloc);
			if (token() == FlowToken::Semicolon) {
				nextToken();
				return stmt;
			}
			return postscriptStmt(stmt);
		}
		default:
		{
			// must be a function/handler (w/o '(' and ')')
			if (line() != sloc.begin.line) {
				Stmt *stmt = new ExprStmt(new CallExpr(lookupOrCreate<Function>(name, true), NULL, CallExpr::Method, sloc), sloc);
				return stmt;
			}

			ListExpr *args = exprList();
			if (!args) {
				delete args;
				Stmt *stmt = new ExprStmt(new CallExpr(lookupOrCreate<Function>(name, true), NULL, CallExpr::Method, sloc), sloc);
				return postscriptStmt(stmt);
			}
			if (token() == FlowToken::Semicolon) {
				nextToken();
				sloc.update(end());
				return new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Method, sloc), sloc);
			}
			sloc.update(end());
			Stmt *stmt = new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Method, sloc), sloc);
			return postscriptStmt(stmt);
		}
	}
}

void ldump(const SourceLocation& sloc, const char *msg)
{
	printf("location %s: { %ld:%ld .. %ld:%ld }\n",
			msg, sloc.begin.line, sloc.begin.column,
			sloc.end.line, sloc.end.column);
}

Stmt *FlowParser::postscriptStmt(Stmt *baseStmt)
{
	FNTRACE();

	if (token() == FlowToken::Semicolon) {
		nextToken();
		return baseStmt;
	}

	//ldump(baseStmt->sourceLocation(), "baseStmt");
	//ldump(lexer_->location(), "current");
	//ldump(lexer_->lastLocation(), "last");
	if (baseStmt->sourceLocation().end.line != line()) {
		return baseStmt;
	}

	switch (token())
	{
		case FlowToken::If:
			return postscriptIfStmt(baseStmt);
		default:
			return baseStmt;
	}
}

Stmt *FlowParser::postscriptIfStmt(Stmt *baseStmt)
{
	FNTRACE();
	// STMT ['if' EXPR] ';'
	SourceLocation sloc(location());

	nextToken(); // 'if'

	Expr *condExpr = expr();
	if (!condExpr) {
		return NULL;
	}

	skip(FlowToken::Semicolon);

	return new CondStmt(condExpr, baseStmt, NULL, sloc.update(end()));
}

Stmt *FlowParser::compoundStmt()
{
	FNTRACE();
	SourceLocation sloc(location());
	consume(FlowToken::Begin);

	CompoundStmt *cs = new CompoundStmt(sloc);

	while (token() == FlowToken::Var)
	{
		Variable *var = varDecl();
		if (!var)
		{
			delete cs;
			return NULL;
		}

		scope()->appendSymbol(var);
	}

	for (;;)
	{
		if (token() == FlowToken::End)
		{
			nextToken();
			cs->sourceLocation().update(end());
			return cs;
		}

		Stmt *s = stmt();
		if (!s)
			break;

		cs->push_back(s);
	}

	delete cs;
	return NULL;
}
// }}}

} // namespace x0
