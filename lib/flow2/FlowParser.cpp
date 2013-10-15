#include <x0/flow2/FlowParser.h>
#include <x0/flow2/FlowLexer.h>
#include <x0/flow2/AST.h>
#include <x0/Utility.h>
#include <unordered_map>

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
#	define TRACE(msg...) XZERO_DEBUG("FlowParser", (level), msg)
#else
#	define FNTRACE() /*!*/
#	define TRACE(msg...) /*!*/
#endif

// {{{ scoped(SCOPED_SYMBOL)
class FlowParser::Scope {
private:
	FlowParser* parser_;
	SymbolTable* table_;
	bool flipped_;

public:
	Scope(FlowParser* parser, SymbolTable* table) :
		parser_(parser),
		table_(table),
		flipped_(false)
	{
		parser_->enter(table_);
	}

	Scope(FlowParser* parser, ScopedSymbol* symbol) :
		parser_(parser),
		table_(symbol->scope()),
		flipped_(false)
	{
		parser_->enter(table_);
	}

	template<typename T>
	Scope(FlowParser* parser, std::unique_ptr<T>& symbol) :
		parser_(parser),
		table_(symbol->scope()),
		flipped_(false)
	{
		parser_->enter(table_);
	}

	bool flip()
	{
		flipped_ = !flipped_;
		return flipped_;
	}

	~Scope()
	{
		parser_->leave();
	}
};
#define scoped(SCOPED_SYMBOL) for (FlowParser::Scope _(this, (SCOPED_SYMBOL)); _.flip(); )
// }}}

FlowParser::FlowParser() :
	lexer_(new FlowLexer()),
	scopeStack_(),
	errorHandler_(),
	backend_(nullptr)
{
}

bool FlowParser::open(const std::string& filename)
{
	if (!lexer_->open(filename))
		return false;

	return false;
}

SymbolTable* FlowParser::enter(SymbolTable* scope)
{
	scopeStack_.push_front(scope);
	return scope;
}

SymbolTable* FlowParser::leave()
{
	SymbolTable* popped = scopeStack_.front();
	scopeStack_.pop_front();
	return popped;
}

std::unique_ptr<Unit> FlowParser::parse()
{
	return unit();
}

// {{{ error mgnt
void FlowParser::reportUnexpectedToken()
{
	reportError("Unexpected token '%s'", token().c_str());
}

void FlowParser::reportError(const std::string& message)
{
	if (!errorHandler_)
		return;

	char buf[1024];
	snprintf(buf, sizeof(buf), "[%04zu:%02zu] %s", lexer_->line(), lexer_->column(), message.c_str());
	errorHandler_(buf);
}
// }}}
// {{{ lexing
FlowToken FlowParser::nextToken() const
{
	return lexer_->nextToken();
}

bool FlowParser::consume(FlowToken value)
{
	if (token() != value) {
		reportError("Unexpected token '%s' (expected: '%s')", token().c_str(), value.c_str());
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

// }}}
// {{{ decls
std::unique_ptr<Unit> FlowParser::unit()
{
	auto unit = std::make_unique<Unit>();

	scoped (unit) {
		while (token() == FlowToken::Import)
			if (!importDecl(unit.get()))
				return nullptr;

		while (auto symbol = decl())
			unit->insert(symbol.get());
	}

	return unit;
}

std::unique_ptr<Symbol> FlowParser::decl()
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

// 'var' IDENT ['=' EXPR] [';']
std::unique_ptr<Variable> FlowParser::varDecl()
{
	FNTRACE();
	FlowLocation loc(lexer_->location());

	if (!consume(FlowToken::Var))
		return nullptr;

	if (!consume(FlowToken::Ident))
		return nullptr;

	std::string name = stringValue();

	if (!consume(FlowToken::Assign))
		return nullptr;

	std::unique_ptr<Expr> initializer = expr();
	if (!initializer)
		return nullptr;

	loc.update(initializer->location().end);
	consumeIf(FlowToken::Semicolon);

	return std::make_unique<Variable>(name, std::move(initializer), loc);
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
			std::string base(lexer_->location().filename);

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

// handlerDecl ::= 'handler' IDENT (';' | [do] stmt)
std::unique_ptr<Handler> FlowParser::handlerDecl()
{
	FlowLocation loc(location());
	nextToken(); // 'handler'

	consume(FlowToken::Ident);
	std::string name = stringValue();
	if (consumeIf(FlowToken::Semicolon)) { // forward-declaration
		loc.update(end());
		return std::make_unique<Handler>(name, loc);
	}

	SymbolTable* st = new SymbolTable(scope());
	scoped (st) {
		std::unique_ptr<Stmt> body = stmt();
		if (!body)
			return nullptr;

		loc.update(body->location().end);

		// forward-declared / previousely -declared?
		if (Handler* handler = scope()->lookup<Handler>(name, Lookup::Self)) {
			if (handler->body() != nullptr) {
				// TODO say where we found the other hand, compared to this one.
				reportError("Redeclaring handler '%s'", handler->name().c_str());
				return nullptr;
			}
			handler->setScope(std::unique_ptr<SymbolTable>(st));
			handler->setBody(std::move(body));
			scope()->removeSymbol(handler);
		}

		return std::make_unique<Handler>(name, std::unique_ptr<SymbolTable>(st), std::move(body), loc);
	}

	return nullptr;
}
// }}}
// {{{ expr
std::unique_ptr<Expr> FlowParser::expr()
{
	std::unique_ptr<Expr> lhs = primaryExpr();
	if (!lhs)
		return nullptr;

	return rhsExpr(std::move(lhs), 0);
}

int binopPrecedence(FlowToken op) {
	static std::unordered_map<FlowToken, int> map = {
		// logical expr
		{ FlowToken::And, 1 },
		{ FlowToken::Or, 1 },
		{ FlowToken::Xor, 1 },

		// rel expr
		{ FlowToken::Equal, 2 },
		{ FlowToken::UnEqual, 2 },
		{ FlowToken::Less, 2 },
		{ FlowToken::Greater, 2 },
		{ FlowToken::LessOrEqual, 2 },
		{ FlowToken::GreaterOrEqual, 2 },

		// add expr
		{ FlowToken::Plus, 2 },
		{ FlowToken::Minus, 2 },

		// mul expr
		{ FlowToken::Mul, 3 },
		{ FlowToken::Div, 3 },
		{ FlowToken::Mod, 3 },

		// bit-wise expr
		{ FlowToken::BitAnd, 5 },
		{ FlowToken::BitOr, 5 },
		{ FlowToken::BitXor, 5 },
	};

	auto i = map.find(op);
	if (i == map.end())
		return -1; // not a binary operator

	return i->second;
}

// rhsExpr ::= (BIN_OP primaryExpr)*
std::unique_ptr<Expr> FlowParser::rhsExpr(std::unique_ptr<Expr> lhs, int lastPrecedence)
{
	// http://en.wikipedia.org/wiki/Operator-precedence_parser

	for (;;) {
		// quit if this is not a binOp *or* its binOp-precedence is lower than the 
		// minimal-binOp-requirement of our caller
		int thisPrecedence = binopPrecedence(token());
		if (thisPrecedence < lastPrecedence)
			return lhs;

		FlowToken binaryOperator = token();
		nextToken();

		std::unique_ptr<Expr> rhs = primaryExpr();
		if (!rhs)
			return nullptr;

		int nextPrecedence = binopPrecedence(token());
		if (thisPrecedence < nextPrecedence) {
			rhs = rhsExpr(std::move(rhs), thisPrecedence + 1);
			if (!rhs)
				return nullptr;
		}

		lhs = std::make_unique<BinaryExpr>(
			binaryOperator, std::move(lhs), std::move(rhs)
		);
	}
}

// primaryExpr ::= NUMBER
//               | STRING
//               | variable
//               | function '(' exprList ')'
//               | '(' expr ')'
std::unique_ptr<Expr> FlowParser::primaryExpr()
{
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

	FlowLocation loc(location());

	switch (token()) {
		case FlowToken::Ident:
			return nullptr; // TODO
		case FlowToken::Boolean: {
			std::unique_ptr<BoolExpr> e = std::make_unique<BoolExpr>(booleanValue(), loc);
			nextToken();
			return std::move(e);
		}
		case FlowToken::RegExp: {
			std::unique_ptr<RegExpExpr> e = std::make_unique<RegExpExpr>(RegExp(stringValue()), loc);
			nextToken();
			return std::move(e);
		}
		case FlowToken::InterpolatedStringFragment:
			return interpolatedStr();
		case FlowToken::String:
		case FlowToken::RawString: {
			std::unique_ptr<StringExpr> e = std::make_unique<StringExpr>(stringValue(), loc);
			nextToken();
			return std::move(e);
		}
		case FlowToken::Number: { // NUMBER [UNIT]
			std::unique_ptr<Expr> e = std::make_unique<StringExpr>(stringValue(), loc);
			nextToken();
			std::string sv(stringValue());
			for (size_t i = 0; units[i].ident; ++i) {
				if (sv == units[i].ident
					|| (sv[sv.size() - 1] == 's' && sv.substr(0, sv.size() - 1) == units[i].ident))
				{
					nextToken(); // UNIT
					e = std::make_unique<BinaryExpr>(FlowToken::Div,
							std::make_unique<BinaryExpr>(FlowToken::Mul,
								std::move(e),
								std::make_unique<NumberExpr>(units[i].nominator, loc)
							),
							std::make_unique<NumberExpr>(units[i].denominator, loc)
					);
					break;
				}
			}

			return std::move(e);
		}
		case FlowToken::IP: {
			std::unique_ptr<IPAddressExpr> e = std::make_unique<IPAddressExpr>(lexer_->ipValue(), loc);
			nextToken();
			return std::move(e);
		}
		case FlowToken::StringType:
		case FlowToken::NumberType:
		case FlowToken::BoolType:
			return castExpr();
		case FlowToken::Begin: { // lambda-like inline function ref
			FlowLocation loc = location();
			auto st = std::make_unique<SymbolTable>(scope());
			enter(st.get());
			std::unique_ptr<Stmt> body = compoundStmt();
			leave();

			if (!body)
				return nullptr;

			loc.update(body->location().end);

			char name[64];
			static unsigned long i = 0;
			snprintf(name, sizeof(name), "__lambda_%lu", ++i);

			Handler* handler = new Handler(name, std::move(st), std::move(body), loc);
			// TODO: add handler to unit's global scope, i.e. via:
			//       - scope()->rootScope()->insert(handler);
			//       - unit_->scope()->insert(handler);
			//       to get free'd
			return std::make_unique<HandlerRefExpr>(handler, loc);
		}
		case FlowToken::RndOpen: {
			nextToken();
			if (token() != FlowToken::RndClose) {
				std::unique_ptr<Expr> e = expr();
				if (!e)
					return nullptr;

				if (token() == FlowToken::Comma) {
					std::unique_ptr<ListExpr> le = std::make_unique<ListExpr>(loc);
					le->push_back(std::move(e));

					do {
						nextToken(); // ','
						if (std::unique_ptr<Expr> elem = expr())
							le->push_back(std::move(elem));
						else
							return nullptr;
					}
					while (token() == FlowToken::Comma);

					e = std::move(le);
				}

				if (!consume(FlowToken::RndClose)) {
					return nullptr;
				}

				e->setLocation(loc.update(end()));
				return e;
			} else {
				if (!consume(FlowToken::RndClose))
					return nullptr;

				// empty list expression
				return std::make_unique<ListExpr>(loc.update(end()));
			}
		}
		default:
			TRACE(1, "Expected primary expression. Got something... else.");
			reportUnexpectedToken();
			return nullptr;
	}
}

std::unique_ptr<Expr> FlowParser::interpolatedStr()
{
	FNTRACE();

	FlowLocation sloc(location());
	std::unique_ptr<Expr> result = std::make_unique<StringExpr>(stringValue(), sloc.update(end()));
	nextToken(); // interpolation start

	std::unique_ptr<Expr> e(expr());
	if (!e)
		return nullptr;

	result = std::make_unique<BinaryExpr>(
		FlowToken::Plus,
		std::move(result),
		std::make_unique<UnaryExpr>(FlowToken::StringType, std::move(e), e->location())
	);

	while (token() == FlowToken::InterpolatedStringFragment) {
		FlowLocation tloc = sloc.update(end());
		result = std::make_unique<BinaryExpr>(
			FlowToken::Plus,
			std::move(result),
			std::make_unique<StringExpr>(stringValue(), tloc)
		);
		nextToken();

		e = expr();
		if (!e)
			return nullptr;

		tloc = e->location();
		result = std::make_unique<BinaryExpr>(
			FlowToken::Plus,
			std::move(result),
			std::make_unique<UnaryExpr>(FlowToken::StringType, std::move(e), tloc)
		);
	}

	if (token() == FlowToken::InterpolatedStringEnd) {
		if (!stringValue().empty()) {
			result = std::make_unique<BinaryExpr>(
				FlowToken::Plus,
				std::move(result),
				std::make_unique<StringExpr>(stringValue(), sloc.update(end()))
			);
		}
		nextToken();
		return result;
	}
	reportUnexpectedToken();
	return nullptr;
}

// castExpr ::= 'int' '(' expr ')'
//            | 'string' '(' expr ')'
//            | 'bool' '(' expr ')'
std::unique_ptr<Expr> FlowParser::castExpr()
{
	return nullptr; // TODO
}

// }}}
// {{{ stmt
std::unique_ptr<Stmt> FlowParser::stmt()
{
	return nullptr; // TODO
}

std::unique_ptr<Stmt> FlowParser::ifStmt()
{
	return nullptr; // TODO
}

std::unique_ptr<Stmt> FlowParser::compoundStmt()
{
	return nullptr; // TODO
}
// }}}

} // namespace x0
