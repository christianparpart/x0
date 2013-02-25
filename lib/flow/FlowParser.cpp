/* <flow/FlowParser.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2010-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/flow/FlowParser.h>
#include <x0/flow/FlowLexer.h>
#include <x0/flow/FlowBackend.h>
#include <x0/flow/Flow.h>
#include <x0/Defines.h>
#include <x0/RegExp.h>
#include <memory>

namespace x0 {

//#define FLOW_DEBUG_PARSER 1

#if defined(FLOW_DEBUG_PARSER)
// {{{ trace
static size_t fnd = 0;
struct fntrace {
	std::string msg_;

	fntrace(const char* msg) : msg_(msg)
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
			fprintf(stderr, "flow: makeOperator: unsupported operator %d\n", static_cast<int>(token));
			return Operator::Undefined;
	}
} // }}}

FlowParser::FlowParser(FlowBackend* b) :
	lexer_(nullptr),
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
	if (errorHandler_) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "[%04zu:%02zu] %s", lexer_->line(), lexer_->column(), message.c_str());

		errorHandler_(buf);
	}
}

bool FlowParser::initialize(std::istream* stream, const std::string& name)
{
	return lexer_->initialize(stream, name);
}

FlowToken FlowParser::nextToken() const
{
#if defined(FLOW_DEBUG_PARSER)
	FlowToken t = lexer_->nextToken();
	printf("token: %s\n", lexer_->dump().c_str());
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
	if (token() != value) {
		reportError("Unexpected token '%s' (expected: '%s')",
				lexer_->tokenString().c_str(),
				lexer_->tokenToString(value).c_str());

		return false;
	}

	nextToken();
	return true;
}

bool FlowParser::consumeIf(FlowToken value)
{
	if (token() == value) {
		nextToken();
		return true;
	} else {
		return false;
	}
}

bool FlowParser::consumeUntil(FlowToken value)
{
	for (;;) {
		if (token() == value) {
			nextToken();
			return true;
		}

		if (token() == FlowToken::Eof)
			return false;

		nextToken();
	}
}

SymbolTable* FlowParser::enter(SymbolTable* scope)
{
	scopeStack_.push_back(scope);
	return scope;
}

SymbolTable* FlowParser::leave()
{
	SymbolTable* popped = scopeStack_.back();
	scopeStack_.pop_back();
	return popped;
}

Unit* FlowParser::parse()
{
	FNTRACE();
	return unit();
}

// {{{ declarations
Unit* FlowParser::unit()
{
	FNTRACE();
	Unit* unit = new Unit();

	enter(&unit->members());

	while (token() == FlowToken::Import)
		if (!importDecl(unit))
			goto err;

	while (Symbol* sym = decl()) {
		TRACE("unit: parsed symbol: %s", sym->name().c_str());
		unit->insert(sym);
	}

	leave();

	if (token() == FlowToken::Eof)
		return unit;

err:
	delete unit;
	return nullptr;
}

Symbol* FlowParser::decl()
{
	FNTRACE();

	switch (token()) {
		case FlowToken::Var:
			return varDecl();
		case FlowToken::Handler:
			return handlerDecl();
		default:
			return nullptr;
	}
}

bool FlowParser::importDecl(Unit* unit)
{
	FNTRACE();

	// 'import' NAME_OR_NAMELIST ['from' PATH] ';'
	nextToken(); // skip 'import'

	std::list<std::string> names;
	if (!importOne(names)) {
		consumeUntil(FlowToken::Semicolon);
		return false;
	}

	while (token() == FlowToken::Comma) {
		nextToken();
		if (!importOne(names)) {
			consumeUntil(FlowToken::Semicolon);
			return false;
		}
	}

	std::string path;
	if (consumeIf(FlowToken::From)) {
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

	consumeIf(FlowToken::Semicolon);
	return true;
}

bool FlowParser::importOne(std::list<std::string>& names)
{
	switch (token()) {
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

			while (token() == FlowToken::Comma) {
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

Variable* FlowParser::varDecl()
{
	FNTRACE();
	SourceLocation sloc(location());

	if (!consume(FlowToken::Var))
		return nullptr;

	if (!consume(FlowToken::Ident))
		return nullptr;

	std::string name = stringValue();

	if (!consume(FlowToken::Assign))
		return nullptr;

	Expr* value = expr();
	if (!value)
		return nullptr;

	sloc.update(end());
	consumeIf(FlowToken::Semicolon);

	return new Variable(scope(), name, value, sloc);
}

Function* FlowParser::handlerDecl()
{
	// handlerDecl ::= 'handler' (';' | [do] stmt)

	FNTRACE();
	SourceLocation sloc(location());
	nextToken(); // 'handler'

	consume(FlowToken::Ident);
	std::string name = stringValue();

	if (consumeIf(FlowToken::Semicolon)) { // forward-declaration
		sloc.update(end());
		return new Function(nullptr, name, nullptr, true, sloc);
	}

	consumeIf(FlowToken::Do);

	SymbolTable* st = new SymbolTable(scope());
	enter(st);
	Stmt* body = stmt();
	leave();

	if (!body) {
		delete st;
		return nullptr;
	}

	sloc.update(end());
	Function* f = static_cast<Function *>(scope()->lookup(name, Lookup::Self));
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
// }}}

// {{{ expressions
Expr* FlowParser::expr() // logicExpr
{
	FNTRACE();
	return assocExpr();
}

Expr* FlowParser::assocExpr()
{
	FNTRACE();
	SourceLocation sloc(location());

	std::unique_ptr<Expr> lhs(logicExpr());
	if (!lhs)
		return nullptr;

	if (!consumeIf(FlowToken::HashRocket))
		return lhs.release();

	std::unique_ptr<Expr> rhs(logicExpr());
	if (!rhs)
		return nullptr;

	std::unique_ptr<ListExpr> assoc(new ListExpr(sloc.update(end())));
	assoc->push_back(lhs.release());
	assoc->push_back(rhs.release());

	return assoc.release();
}

Expr* FlowParser::logicExpr()
{
	FNTRACE();
	SourceLocation sloc(location());

	Expr* left = relExpr();
	if (!left)
		return nullptr;

	for (;;) {
		switch (token()) {
			case FlowToken::And:
			case FlowToken::Or:
			case FlowToken::Xor: {
				Operator op = makeOperator(token());
				nextToken();

				Expr* right = relExpr();
				if (!right) {
					delete left;
					return nullptr;
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

Expr* FlowParser::relExpr() // addExpr ((== != <= >= < > =^ =$ =~ 'in') addExpr)*
{
	FNTRACE();
	SourceLocation sloc(location());
	Expr* left = addExpr();

	for (;;) {
		switch (token()) {
			case FlowToken::Equal:
			case FlowToken::UnEqual:
			case FlowToken::LessOrEqual:
			case FlowToken::GreaterOrEqual:
			case FlowToken::Less:
			case FlowToken::Greater:
			case FlowToken::PrefixMatch:
			case FlowToken::SuffixMatch:
			case FlowToken::RegexMatch:
			case FlowToken::In: {
				Operator op = makeOperator(token());
				nextToken();
				Expr* right = addExpr();
				if (!right) {
					delete left;
					return nullptr;
				}
				left = new BinaryExpr(op, left, right, sloc.update(end()));
			}
			default:
				return left;
		}
	}
}

Expr* FlowParser::addExpr() // mulExpr (('+' | '-') mulExpr)*
{
	FNTRACE();
	SourceLocation sloc(location());
	Expr* left = mulExpr();
	if (!left)
		return nullptr;

	for (;;) {
		switch (token()) {
			case FlowToken::Plus:
			case FlowToken::Minus: {
				Operator op = makeOperator(token());
				nextToken();

				Expr* right = mulExpr();
				if (!right) {
					delete left;
					return nullptr;
				}

				left = new BinaryExpr(op, left, right, sloc.update(end()));
				break;
			}
			default:
				return left;
		}
	}
}

Expr* FlowParser::mulExpr()
{
	// powExpr (('*' | '/' | shl | shr) powExpr)*

	FNTRACE();
	SourceLocation sloc(location());
	Expr* left = negExpr();
	if (!left)
		return nullptr;

	for (;;) {
		switch (token()) {
			case FlowToken::Mul:
			case FlowToken::Div:
			case FlowToken::Mod:
			case FlowToken::Shl:
			case FlowToken::Shr: {
				Operator op = makeOperator(token());
				nextToken();

				Expr* right = negExpr();
				if (!right) {
					delete left;
					return nullptr;
				}
				left = new BinaryExpr(op, left, right, sloc.update(end()));
				break;
			}
			default:
				return left;
		}
	}
}

Expr* FlowParser::negExpr()
{
	// negExpr ::= (('!' | '-' | '+') negExpr)+ | powExpr

	FNTRACE();
	SourceLocation sloc(location());

	if (FlowTokenTraits::isUnaryOp(token())) {
		auto op = makeUnaryOperator(token());
		nextToken();
		std::unique_ptr<Expr> e(negExpr());
		return e ? new UnaryExpr(op, e.release(), sloc.update(end())) : nullptr;
	} else {
		return powExpr();
	}
}

Expr* FlowParser::powExpr()
{
	// primaryExpr ('**' primaryExpr)*
	FNTRACE();
	SourceLocation sloc(location());
	Expr* left = primaryExpr();
	if (!left)
		return nullptr;

	for (;;) {
		switch (token()) {
			case FlowToken::Pow: {
				Operator op = makeOperator(token());
				nextToken();

				Expr* right = primaryExpr();
				if (!right) {
					delete left;
					return nullptr;
				}
				left = new BinaryExpr(op, left, right, sloc.update(end()));
				break;
			}
			default:
				return left;
		}
	}
}

Expr* FlowParser::primaryExpr()
{
	// primaryExpr ::= subExpr UNIT?
	FNTRACE();
	SourceLocation sloc(location());

	Expr* left = subExpr();

	if (token() == FlowToken::Ident) {
		static struct {
			const char* ident;
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
			{ nullptr, 1, 1 }
		};

		std::string sv(stringValue());

		for (size_t i = 0; units[i].ident; ++i) {
			if (sv == units[i].ident
				|| (sv[sv.size() - 1] == 's' && sv.substr(0, sv.size() - 1) == units[i].ident))
			{
				nextToken();
				Expr* nominator = new NumberExpr(units[i].nominator, SourceLocation());
				Expr* denominator = new NumberExpr(units[i].denominator, SourceLocation());
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

Expr* FlowParser::subExpr()
{
	// subExpr ::= literalExpr ['=>' expr]
	//           | symbolExpr
	//           | castExpr
	//			 | '(' expr ')'
	//			 | '{' stmt '}'

	FNTRACE();
	SourceLocation sloc(location());

	switch (token()) {
		case FlowToken::Begin: { // lambda-like inline function ref
			SymbolTable* st = new SymbolTable(scope());
			enter(st);
			Stmt* body = compoundStmt();
			leave();

			char name[64];
			static unsigned long i = 0;
			snprintf(name, sizeof(name), "__lambda_%ld", ++i);

			Function* f = new Function(st, name, body, true, body->sourceLocation());
			return new FunctionRefExpr(f, body->sourceLocation());
		}
		case FlowToken::RndOpen: {
			nextToken();
			if (token() != FlowToken::RndClose) {
				Expr* e = expr();

				if (token() == FlowToken::Comma) {
					ListExpr* le = new ListExpr(sloc);
					le->push_back(e);
					e = le;

					do {
						nextToken(); // skip comma
						Expr* elem = expr();
						if (elem)
							le->push_back(elem);
						else {
							delete e;
							return nullptr;
						}
					}
					while (token() == FlowToken::Comma);
				}

				if (!consume(FlowToken::RndClose)) {
					delete e;
					return nullptr;
				}

				e->setSourceLocation(sloc.update(end()));
				return e;
			} else {
				if (!consume(FlowToken::RndClose))
					return nullptr;

				return new ListExpr(sloc.update(end()));
			}
		}
		case FlowToken::BrOpen: // [ expr [',' expr]* ]
			return hashExpr();
		case FlowToken::StringType:
		case FlowToken::IntType:
		case FlowToken::BoolType:
			return castExpr();
		case FlowToken::Ident:
			return symbolExpr();
		default:
			return literalExpr();
	}
}

inline FlowValue::Type toType(FlowToken token)
{
	switch (token) {
		case FlowToken::BoolType:
			return FlowValue::BOOLEAN;
		case FlowToken::IntType:
			return FlowValue::NUMBER;
		case FlowToken::StringType:
			return FlowValue::STRING;
		default:
			return FlowValue::VOID;
	}
}

Expr* FlowParser::castExpr()
{
	FNTRACE();
	SourceLocation sloc(location());

	FlowValue::Type targetType = toType(token());
	nextToken();

	if (targetType == FlowValue::VOID) {
		reportError("Invalid cast.");
		return nullptr;
	}

	consume(FlowToken::RndOpen);
	std::unique_ptr<Expr> expr(logicExpr());
	consume(FlowToken::RndClose);

	if (!expr)
		return nullptr;

	return new CastExpr(targetType, expr.release(), sloc.update(end()));
}

Expr* FlowParser::literalExpr()
{
	// LiteralExpr ::= number | string | bool | regexp | ip

	FNTRACE();
	SourceLocation sloc(location());

	switch (token()) {
		case FlowToken::RegExp: {
			Expr* e = new RegExpExpr(RegExp(stringValue()), sloc.update(end()));
			nextToken();
			return e;
		}
		case FlowToken::String:
		case FlowToken::RawString: {
			Expr* e = new StringExpr(stringValue(), sloc.update(end()));
			nextToken();
			return e;
		}
		case FlowToken::InterpolatedStringFragment: {
			return interpolatedStr();
		}
		case FlowToken::Boolean: {
			Expr* e = new BoolExpr(booleanValue(), sloc.update(end()));
			nextToken();
			return e;
		}
		case FlowToken::Number: {
			Expr* e = new NumberExpr(numberValue(), sloc.update(end()));
			nextToken();
			return e;
		}
		case FlowToken::IP: {
			Expr* e = new IPAddressExpr(lexer_->ipValue(), sloc.update(end()));
			nextToken();
			return e;
		}
		default:
			reportUnexpectedToken();
			return nullptr;
	}
}

Expr* FlowParser::interpolatedStr()
{
	FNTRACE();

	SourceLocation sloc(location());
	std::unique_ptr<Expr> result(new StringExpr(stringValue(), sloc.update(end())));
	nextToken();
	std::unique_ptr<Expr> e(expr());
	if (!e)
		return nullptr;

	result.reset(new BinaryExpr(
		Operator::Plus,
		result.release(),
		new CastExpr(FlowValue::STRING, e.get(), e->sourceLocation()),
		sloc.update(end())
	));
	e.release();

	while (token() == FlowToken::InterpolatedStringFragment) {
		result.reset(new BinaryExpr(
			Operator::Plus,
			result.release(),
			new StringExpr(stringValue(), sloc.update(end())),
			sloc.update(end())
		));
		nextToken();

		e.reset(expr());
		if (!e)
			return nullptr;

		result.reset(new BinaryExpr(
			Operator::Plus,
			result.release(),
			new CastExpr(FlowValue::STRING, e.get(), e->sourceLocation()),
			sloc.update(end())
		));
		e.release();
	}

	if (token() == FlowToken::InterpolatedStringEnd) {
		if (!stringValue().empty()) {
			result.reset(new BinaryExpr(
				Operator::Plus,
				result.release(),
				new StringExpr(stringValue(), sloc.update(end())),
				sloc.update(end())
			));
		}
		nextToken();
		return result.release();
	}

	reportUnexpectedToken();
	return nullptr;
}

Expr* FlowParser::symbolExpr()
{
	// symbolExpr ::= variableExpr | functionCallExpr
	//            ::= NAME
	//              | NAME '(' exprList ')'

	FNTRACE();
	SourceLocation sloc(location());

	std::string name = stringValue();
	if (!consume(FlowToken::Ident))
		return nullptr;

	if (token() == FlowToken::RndOpen) {
		// function call expression
		nextToken();
		std::unique_ptr<ListExpr> args(nullptr);
		if (token() != FlowToken::RndClose) {
			args.reset(exprList());
			if (!args || !consume(FlowToken::RndClose)) {
				return nullptr;
			}
		} else if (!consume(FlowToken::RndClose)) {
			return nullptr;
		}
		sloc.update(end());
		return new CallExpr(lookupOrCreate<Function>(name), args.release(), CallExpr::Method, sloc);
	} else {
		// variable / handler ref expression
		sloc.update(end());

		if (Symbol* symbol = scope()->lookup(name, Lookup::All)) {
			switch (symbol->type()) {
				case Symbol::VARIABLE:
					TRACE("Creating VariableExpr: '%s'\n", name.c_str());
					return new VariableExpr(static_cast<Variable*>(symbol), sloc);
				case Symbol::FUNCTION:
					TRACE("Creating FunctionRefExpr: '%s'\n", name.c_str());
					return new FunctionRefExpr(static_cast<Function*>(symbol), sloc);
				default:
					reportError("Invalid reference to symbol '%s'", name.c_str());
					return nullptr;
			}
		} else if (backend_->isProperty(name)) {
			TRACE("Creating Variable(Expr) to native property: '%s'", name.c_str());
			Variable* v = new Variable(name, sloc);
			scopeStack_.front()->appendSymbol(v);
			return new VariableExpr(v, sloc);
		} else {
			TRACE("Creating Function(RefExpr): '%s'\n", name.c_str());
			Function* f = new Function(name, true);
			scopeStack_.front()->appendSymbol(f); // register forward to global-scope
			return new FunctionRefExpr(f, sloc);
		}
	}
}

ListExpr* FlowParser::exprList() // expr (',' expr)
{
	FNTRACE();
	SourceLocation sloc(location());

	if (Expr* e = expr()) {
		std::unique_ptr<ListExpr> list(new ListExpr(sloc));
		list->push_back(e);

		while (consumeIf(FlowToken::Comma)) {
			if ((e = expr()) != nullptr)
				list->push_back(e);
			else
				return nullptr;
		}

		list->sourceLocation().update(end());
		return list.release();
	}
	return nullptr;
}

Expr* FlowParser::hashExpr()
{
	// '[' (expr (',' expr)*)? ']'

	FNTRACE();
	SourceLocation sloc(location());

	if (!consume(FlowToken::BrOpen))
		return nullptr;

	if (consumeIf(FlowToken::BrClose))
		return new ListExpr(sloc.update(end()));

	std::unique_ptr<ListExpr> e(exprList());
	if (!consume(FlowToken::BrClose)) {
		return nullptr;
	}
	e->setSourceLocation(sloc.update(end()));
	return e.release();
}
// }}}

//{{{ statements
Stmt* FlowParser::stmt()
{
	FNTRACE();

	switch (token()) {
		case FlowToken::If:
			return ifStmt();
		case FlowToken::Begin:
			return compoundStmt();
		case FlowToken::Ident:
			return callStmt();
		case FlowToken::Semicolon: {
			SourceLocation sloc(location());
			nextToken();
			return new CompoundStmt(sloc.update(end()));
		}
		default:
			reportError("Unexpected token '%s'. Expected a statement instead.", lexer_->tokenToString(token()).c_str());
			return nullptr;
	}
}

Stmt* FlowParser::ifStmt()
{
	// ifStmt ::= 'if' expr ['then'] stmt ['else' stmt]
	FNTRACE();
	SourceLocation sloc(location());

	consume(FlowToken::If);
	std::unique_ptr<Expr> cond(expr());
	consumeIf(FlowToken::Then);

	std::unique_ptr<Stmt> thenStmt(stmt());
	if (!thenStmt)
		return nullptr;

	std::unique_ptr<Stmt> elseStmt;

	if (consumeIf(FlowToken::Else)) {
		elseStmt.reset(stmt());
		if (!elseStmt) {
			return nullptr;
		}
	}

	return new CondStmt(cond.release(), thenStmt.release(), elseStmt.release(), sloc.update(end()));
}

Stmt* FlowParser::callStmt()
{
	// callStmt ::= NAME ['(' exprList ')' | exprList] (';' | LF)
	// NAME may be a variable-, function- or handler-name.

	FNTRACE();
	SourceLocation sloc(location());
	//sloc.dump(token().c_str());

	std::string name = stringValue();
	if (!consume(FlowToken::Ident))
		return nullptr;

	switch (token()) {
		case FlowToken::If:
			return postscriptStmt(new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), nullptr/*args*/, CallExpr::Undefined, sloc), sloc));
		case FlowToken::Unless:
			return postscriptStmt(new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), nullptr/*args*/, CallExpr::Undefined, sloc), sloc));
		case FlowToken::Semicolon:
			// must be a function/handler
			nextToken();
			sloc.update(end());
			return new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), nullptr/*args*/, CallExpr::Undefined, sloc), sloc);
		case FlowToken::Assign: {
			// must be a writable variable.
			nextToken(); // skip '='
			Expr* e = expr();
			if (!e)
				return nullptr;

			sloc.update(end());

			ListExpr* args = new ListExpr(e->sourceLocation());
			args->push_back(e);

			return new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Assignment, sloc), sloc);
		}
		case FlowToken::RndOpen: {
			// must be a function/handler

			nextToken();
			ListExpr* args = nullptr;
			if (token() != FlowToken::RndClose) {
				args = exprList();
				if (!args) {
					delete args;
					return nullptr;
				}
			}
			if (!consume(FlowToken::RndClose)) {
				delete args;
				return nullptr;
			}
			sloc.update(end());
			Stmt* stmt = new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Method, sloc), sloc);
			if (token() == FlowToken::Semicolon) {
				nextToken();
				return stmt;
			}
			return postscriptStmt(stmt);
		}
		default: {
			// must be a function/handler (w/o '(' and ')')
			if (line() != sloc.begin.line) {
				Stmt* stmt = new ExprStmt(new CallExpr(lookupOrCreate<Function>(name, true), nullptr, CallExpr::Method, sloc), sloc);
				return stmt;
			}

			ListExpr* args = exprList();
			if (!args) {
				delete args;
				Stmt* stmt = new ExprStmt(new CallExpr(lookupOrCreate<Function>(name, true), nullptr, CallExpr::Method, sloc), sloc);
				return postscriptStmt(stmt);
			}
			if (token() == FlowToken::Semicolon) {
				nextToken();
				sloc.update(end());
				return new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Method, sloc), sloc);
			}
			sloc.update(end());
			Stmt* stmt = new ExprStmt(new CallExpr(lookupOrCreate<Function>(name), args, CallExpr::Method, sloc), sloc);
			return postscriptStmt(stmt);
		}
	}
}

void ldump(const SourceLocation& sloc, const char* msg)
{
	printf("location %s: { %zu:%zu .. %zu:%zu }\n",
			msg, sloc.begin.line, sloc.begin.column,
			sloc.end.line, sloc.end.column);
}

Stmt* FlowParser::postscriptStmt(Stmt* baseStmt)
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

	switch (token()) {
		case FlowToken::If:
			return postscriptIfStmt(baseStmt);
		case FlowToken::Unless:
			return postscriptUnlessStmt(baseStmt);
		default:
			return baseStmt;
	}
}

Stmt* FlowParser::postscriptIfStmt(Stmt* baseStmt)
{
	FNTRACE();
	// STMT ['if' EXPR] ';'
	SourceLocation sloc(location());

	nextToken(); // 'if'

	Expr* condExpr = expr();
	if (!condExpr)
		return nullptr;

	consumeIf(FlowToken::Semicolon);

	return new CondStmt(condExpr, baseStmt, nullptr, sloc.update(end()));
}

Stmt* FlowParser::postscriptUnlessStmt(Stmt* baseStmt)
{
	FNTRACE();
	// STMT ['unless' EXPR] ';'
	SourceLocation sloc(location());

	nextToken(); // 'unless'

	Expr* condExpr = expr();
	if (!condExpr)
		return nullptr;

	condExpr = new UnaryExpr(Operator::Not, condExpr, sloc);

	consumeIf(FlowToken::Semicolon);

	return new CondStmt(condExpr, baseStmt, nullptr, sloc.update(end()));
}

Stmt* FlowParser::compoundStmt()
{
	FNTRACE();
	SourceLocation sloc(location());
	consume(FlowToken::Begin);

	std::unique_ptr<CompoundStmt> cs(new CompoundStmt(sloc));

	while (token() == FlowToken::Var) {
		if (Variable* var = varDecl())
			scope()->appendSymbol(var);
		else
			return nullptr;
	}

	for (;;) {
		if (consumeIf(FlowToken::End)) {
			cs->sourceLocation().update(end());
			return cs.release();
		}

		if (Stmt* s = stmt())
			cs->push_back(s);
		else
			break;
	}

	return nullptr;
}
// }}}

} // namespace x0
