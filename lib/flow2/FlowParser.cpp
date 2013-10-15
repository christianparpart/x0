#include <x0/flow2/FlowParser.h>
#include <x0/flow2/FlowLexer.h>
#include <x0/flow2/AST.h>
#include <x0/Utility.h>

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
	FlowLocation sloc(lexer_->location());

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

	sloc.update(initializer->location().end);
	consumeIf(FlowToken::Semicolon);

	return std::make_unique<Variable>(name, std::move(initializer), sloc);
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
	FlowLocation sloc(location());
	nextToken(); // 'handler'

	consume(FlowToken::Ident);
	std::string name = stringValue();
	if (consumeIf(FlowToken::Semicolon)) { // forward-declaration
		sloc.update(end());
		return std::make_unique<Handler>(name, sloc);
	}

	SymbolTable* st = new SymbolTable(scope());
	scoped (st) {
		std::unique_ptr<Stmt> body = stmt();
		if (!body)
			return nullptr;

		sloc.update(body->location().end);

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

		return std::make_unique<Handler>(name, std::unique_ptr<SymbolTable>(st), std::move(body), sloc);
	}

	return nullptr;
}
// }}}
// {{{ expr
std::unique_ptr<Expr> FlowParser::expr()
{
	return nullptr; // TODO
}
// }}}
// {{{ stmt
std::unique_ptr<Stmt> FlowParser::stmt()
{
	return nullptr; // TODO
}
// }}}

} // namespace x0
