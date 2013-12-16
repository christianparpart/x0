#include <x0/flow2/vm/NativeCallback.h>
#include <x0/flow2/FlowParser.h>
#include <x0/flow2/FlowLexer.h>
#include <x0/flow2/vm/Runtime.h>
#include <x0/flow2/AST.h>
#include <x0/Utility.h>
#include <x0/DebugLogger.h>
#include <unordered_map>
#include <unistd.h>

enum class OpSig {
    Invalid,
    BoolBool,
    NumNum,
    StringString,
    StringRegexp,
    IpIp,
    IpCidr,
    CidrCidr,
};

namespace std {
    template<> struct hash<OpSig> {
        uint32_t operator()(OpSig v) const noexcept { return static_cast<uint32_t>(v); }
    };
}

namespace x0 {

using FlowVM::Opcode;

#define FLOW_DEBUG_PARSER 1

#if defined(FLOW_DEBUG_PARSER)
// {{{ trace
static size_t fnd = 0;
struct fntrace {
	std::string msg_;

	fntrace(const char* msg) : msg_(msg)
	{
		size_t i = 0;
		char fmt[1024];

		for (i = 0; i < 2 * fnd; ) {
			fmt[i++] = ' ';
			fmt[i++] = ' ';
		}
		fmt[i++] = '-';
		fmt[i++] = '>';
		fmt[i++] = ' ';
		strcpy(fmt + i, msg_.c_str());

		XZERO_DEBUG("FlowParser", 5, "%s", fmt);
		++fnd;
	}

	~fntrace() {
		--fnd;

		size_t i = 0;
		char fmt[1024];

		for (i = 0; i < 2 * fnd; ) {
			fmt[i++] = ' ';
			fmt[i++] = ' ';
		}
		fmt[i++] = '<';
		fmt[i++] = '-';
		fmt[i++] = ' ';
		strcpy(fmt + i, msg_.c_str());

		XZERO_DEBUG("FlowParser", 5, "%s", fmt);
	}
};
// }}}
#	define FNTRACE() fntrace _(__PRETTY_FUNCTION__)
#	define TRACE(level, msg...) XZERO_DEBUG("FlowParser", (level), msg)
#else
#	define FNTRACE() /*!*/
#	define TRACE(level, msg...) /*!*/
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

	Scope(FlowParser* parser, std::unique_ptr<SymbolTable>& table) :
		parser_(parser),
		table_(table.get()),
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

FlowParser::FlowParser(FlowVM::Runtime* runtime) :
	lexer_(new FlowLexer()),
	scopeStack_(nullptr),
	runtime_(runtime),
	errorHandler(),
	importHandler()
{
	//enter(new SymbolTable(nullptr, "global"));
}

FlowParser::~FlowParser()
{
	//leave();
    //assert(scopeStack_ == nullptr && "scopeStack not properly unwind. probably a bug.");
}

bool FlowParser::open(const std::string& filename)
{
	if (!lexer_->open(filename))
		return false;

	return true;
}

SymbolTable* FlowParser::enter(SymbolTable* scope)
{
    //printf("Parser::enter(): new top: %p \"%s\" (outer: %p \"%s\")\n", scope, scope->name().c_str(), scopeStack_, scopeStack_ ? scopeStack_->name().c_str() : "");

	scope->setOuterTable(scopeStack_);
	scopeStack_ = scope;

	return scope;
}

SymbolTable* FlowParser::leave()
{
	SymbolTable* popped = scopeStack_;
	scopeStack_ = scopeStack_->outerTable();

    //printf("Parser::leave(): new top: %p \"%s\" (outer: %p \"%s\")\n", popped, popped->name().c_str(), scopeStack_, scopeStack_ ? scopeStack_->name().c_str() : "");

	return popped;
}

std::unique_ptr<Unit> FlowParser::parse()
{
	return unit();
}

// {{{ type system
/**
 * Computes VM opcode for binary operation.
 *
 * @param token operator between left and right expression
 * @param left left hand expression
 * @param right right hand expression
 *
 * @return opcode that matches given expressions operator or EXIT if operands incompatible.
 */
FlowVM::Opcode makeOperator(FlowToken token , Expr* left, Expr* right)
{
    // (bool, bool)     == !=
    // (num, num)       + - * / % ** << >> & | ^ and or xor
    // (string, string) == != <= >= < > in
    // (string, regex)  =~
    // (ip, ip)         == !=
    // (ip, cidr) in    in
    // (cidr, cidr)     == != in

    auto isString = [](FlowType t) { return t == FlowType::String; };
    auto isNumber = [](FlowType t) { return t == FlowType::Number; };
    auto isBool   = [](FlowType t) { return t == FlowType::Boolean; };
    auto isIPAddr = [](FlowType t) { return t == FlowType::IPAddress; };
    auto isCidr   = [](FlowType t) { return t == FlowType::Cidr; };
    auto isRegExp = [](FlowType t) { return t == FlowType::RegExp; };

    FlowType leftType = left->getType();
    FlowType rightType = right->getType();

    OpSig opsig = OpSig::Invalid;
    if (isBool(leftType) && isBool(rightType)) opsig = OpSig::BoolBool;
    else if (isNumber(leftType) && isNumber(rightType)) opsig = OpSig::NumNum;
    else if (isString(leftType) && isString(rightType)) opsig = OpSig::StringString;
    else if (isString(leftType) && isRegExp(rightType)) opsig = OpSig::StringRegexp;
    else if (isIPAddr(leftType) && isIPAddr(rightType)) opsig = OpSig::IpIp;
    else if (isIPAddr(leftType) && isCidr(rightType)) opsig = OpSig::IpCidr;
    else if (isCidr(leftType) && isCidr(rightType)) opsig = OpSig::CidrCidr;

    static const std::unordered_map<OpSig, std::unordered_map<FlowToken, Opcode>> ops = {
        {OpSig::BoolBool, {
            {FlowToken::Equal, Opcode::NCMPEQ},
            {FlowToken::UnEqual, Opcode::NCMPNE},
            {FlowToken::And, Opcode::NAND},
            {FlowToken::Or, Opcode::NOR},
            {FlowToken::Xor, Opcode::NXOR},
        }},
        {OpSig::NumNum, {
            {FlowToken::Plus, Opcode::NADD},
            {FlowToken::Minus, Opcode::NSUB},
            {FlowToken::Mul, Opcode::NMUL},
            {FlowToken::Div, Opcode::NDIV},
            {FlowToken::Mod, Opcode::NREM},
            {FlowToken::Equal, Opcode::NCMPEQ},
            {FlowToken::UnEqual, Opcode::NCMPNE},
            {FlowToken::LessOrEqual, Opcode::NCMPLT},
            {FlowToken::GreaterOrEqual, Opcode::NCMPGT},
            {FlowToken::Less, Opcode::NCMPLE},
            {FlowToken::Greater, Opcode::NCMPGE},
            {FlowToken::BitAnd, Opcode::NAND},
            {FlowToken::BitOr, Opcode::NOR},
            {FlowToken::BitXor, Opcode::NXOR},
        }},
        {OpSig::StringString, {
            {FlowToken::Plus, Opcode::SADD},
            {FlowToken::Equal, Opcode::SCMPEQ},
            {FlowToken::UnEqual, Opcode::SCMPNE},
            {FlowToken::LessOrEqual, Opcode::SCMPLE},
            {FlowToken::GreaterOrEqual, Opcode::SCMPGE},
            {FlowToken::Less, Opcode::SCMPLT},
            {FlowToken::Greater, Opcode::SCMPGT},
            {FlowToken::PrefixMatch, Opcode::SCMPBEG},
            {FlowToken::SuffixMatch, Opcode::SCMPEND},
            {FlowToken::In, Opcode::SCONTAINS},
        }},
        {OpSig::StringRegexp, {
            {FlowToken::RegexMatch, Opcode::SREGMATCH},
        }},
        {OpSig::IpIp, {
            {FlowToken::Equal, Opcode::NOP},
            {FlowToken::UnEqual, Opcode::NOP},
        }},
        {OpSig::IpCidr, {
            {FlowToken::In, Opcode::NOP},
        }},
        {OpSig::CidrCidr, {
            {FlowToken::Equal, Opcode::NOP},
            {FlowToken::UnEqual, Opcode::NOP},
            {FlowToken::In, Opcode::NOP},
        }},
    };

    auto a = ops.find(opsig);
    if (a == ops.end())
        return Opcode::EXIT;

    auto b = a->second.find(token);
    if (b == a->second.end())
        return Opcode::EXIT;

    return b->second;
}

Opcode makeOperator(FlowToken token, Expr* e)
{
    static const std::unordered_map<FlowType, std::unordered_map<FlowToken, Opcode>> ops = {
        {FlowType::Number, {
            {FlowToken::Not, Opcode::NCMPZ},
            {FlowToken::Minus, Opcode::NNEG},
            {FlowToken::StringType, Opcode::I2S},
            {FlowToken::BoolType, Opcode::NCMPZ},
            {FlowToken::NumberType, Opcode::NOP},
        }},
        {FlowType::Boolean, {
            {FlowToken::Not, Opcode::NNEG},
            {FlowToken::BoolType, Opcode::NOP},
        }},
        {FlowType::String, {
            {FlowToken::Not, Opcode::SISEMPTY},
            {FlowToken::NumberType, Opcode::S2I},
            {FlowToken::StringType, Opcode::NOP},
        }},
#if 0 // TODO
        {FlowType::RegExp, {
            {FlowToken::StringType, Opcode::REGEXP2S},
        }},
        {FlowType::IPAddress, {
            {FlowToken::StringType, Opcode::IP2S},
            {FlowToken::IP, Opcode::NOP},
        }},
        {FlowType::Cidr, {
            {FlowToken::StringType, Opcode::CIDR2S},
        }},
#endif
    };

    FlowType type = e->getType();

    auto a = ops.find(type);
    if (a == ops.end())
        return Opcode::EXIT;

    auto b = a->second.find(token);
    if (b == a->second.end())
        return Opcode::EXIT;

    return b->second;
}
// }}}
// {{{ error mgnt
void FlowParser::reportUnexpectedToken()
{
	reportError("Unexpected token '%s'", token().c_str());
}

void FlowParser::reportError(const std::string& message)
{

	if (!errorHandler) {
		char buf[1024];
		int n = snprintf(buf, sizeof(buf), "[%04zu:%02zu] %s\n", lexer_->line(), lexer_->column(), message.c_str());
		write(2, buf, n);
		return;
	}

	char buf[1024];
	snprintf(buf, sizeof(buf), "[%04zu:%02zu] %s", lexer_->line(), lexer_->column(), message.c_str());
	errorHandler(buf);
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
        importRuntime();

		while (token() == FlowToken::Import) {
			if (!importDecl(unit.get())) {
				return nullptr;
            }
        }

		while (std::unique_ptr<Symbol> symbol = decl()) {
			scope()->appendSymbol(std::move(symbol));
        }
	}

	return unit;
}

void FlowParser::importRuntime()
{
    if (runtime_) {
		//printf("runtime, %zu builtins\n", runtime_->builtins().size());
		for (const auto& builtin: runtime_->builtins()) {
            //printf("%s\n", builtin.signature().to_s().c_str());

			if (builtin.isHandler()) {
				createSymbol<BuiltinHandler>(builtin.signature());
            } else {
				createSymbol<BuiltinFunction>(builtin.signature());
            }
		}
	}
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

	for (auto i = names.begin(), e = names.end(); i != e; ++i) {
		if (importHandler && !importHandler(*i, path))
			return false;

		unit->import(*i, path);
	}

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
	FNTRACE();

	FlowLocation loc(location());
	nextToken(); // 'handler'

	consume(FlowToken::Ident);
	std::string name = stringValue();
	if (consumeIf(FlowToken::Semicolon)) { // forward-declaration
		loc.update(end());
		return std::make_unique<Handler>(name, loc);
	}

	std::unique_ptr<SymbolTable> st = enterScope("handler-" + name);
	std::unique_ptr<Stmt> body = stmt();
	leaveScope();

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
		handler->implement(std::move(st), std::move(body));
        handler->owner()->removeSymbol(handler);
        return std::unique_ptr<Handler>(handler);
	}

	return std::make_unique<Handler>(name, std::move(st), std::move(body), loc);
}
// }}}
// {{{ expr
std::unique_ptr<Expr> FlowParser::expr()
{
	std::unique_ptr<Expr> lhs = powExpr();
	if (!lhs)
		return nullptr;

	return rhsExpr(std::move(lhs), 0);
}

int binopPrecedence(FlowToken op) {
	static const std::unordered_map<FlowToken, int> map = {
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
		{ FlowToken::PrefixMatch, 2 },
		{ FlowToken::SuffixMatch, 2 },
		{ FlowToken::RegexMatch, 2 },
		{ FlowToken::In, 2 },

		// add expr
		{ FlowToken::Plus, 2 },
		{ FlowToken::Minus, 2 },

		// mul expr
		{ FlowToken::Mul, 3 },
		{ FlowToken::Div, 3 },
		{ FlowToken::Mod, 3 },
		{ FlowToken::Shl, 3 },
		{ FlowToken::Shr, 3 },

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
	FNTRACE();

	for (;;) {
		// quit if this is not a binOp *or* its binOp-precedence is lower than the 
		// minimal-binOp-requirement of our caller
		int thisPrecedence = binopPrecedence(token());
		if (thisPrecedence <= lastPrecedence)
			return lhs;

		FlowToken binaryOperator = token();
		nextToken();

		std::unique_ptr<Expr> rhs = powExpr();
		if (!rhs)
			return nullptr;

		int nextPrecedence = binopPrecedence(token());
		if (thisPrecedence < nextPrecedence) {
			rhs = rhsExpr(std::move(rhs), thisPrecedence + 0);
			if (!rhs)
				return nullptr;
		}

        Opcode opc = makeOperator(binaryOperator, lhs.get(), rhs.get());
        if (opc == Opcode::EXIT) {
            reportError("Type error in binary expression (%s versus %s).",
                        tos(lhs->getType()).c_str(), tos(rhs->getType()).c_str());
            return nullptr;
        }

		lhs = std::make_unique<BinaryExpr>(opc, std::move(lhs), std::move(rhs));
	}
}

std::unique_ptr<Expr> FlowParser::powExpr()
{
	// powExpr ::= primaryExpr ('**' powExpr)*
	FNTRACE();

	FlowLocation sloc(location());
	std::unique_ptr<Expr> left = primaryExpr();
	if (!left)
		return nullptr;

	while (token() == FlowToken::Pow) {
		nextToken();

		std::unique_ptr<Expr> right = powExpr();
		if (!right)
			return nullptr;

        auto opc = makeOperator(FlowToken::Pow, left.get(), right.get());
        if (opc == Opcode::EXIT) {
            reportError("Type error in binary expression (%s versus %s).",
                        tos(left->getType()).c_str(), tos(right->getType()).c_str());
            return nullptr;
        }

		left = std::make_unique<BinaryExpr>(opc, std::move(left), std::move(right));
	}

	return left;
}

// primaryExpr ::= NUMBER
//               | STRING
//               | variable
//               | function '(' exprList ')'
//               | '(' expr ')'
std::unique_ptr<Expr> FlowParser::primaryExpr()
{
	FNTRACE();

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
		case FlowToken::Ident: {
			std::string name = stringValue();
			nextToken();

			Symbol* symbol = scope()->lookup(name, Lookup::All);
			if (!symbol) {
                // XXX assume that given symbol is a auto forward-declared handler.
                Handler* href = (Handler*) globalScope()->appendSymbol(std::make_unique<Handler>(name, loc));
				return std::make_unique<HandlerRefExpr>(href, loc);
			}

			if (auto variable = dynamic_cast<Variable*>(symbol))
				return std::make_unique<VariableExpr>(variable, loc);

			if (auto handler = dynamic_cast<Handler*>(symbol))
				return std::make_unique<HandlerRefExpr>(handler, loc);

			if (symbol->type() == Symbol::BuiltinFunction) {
				if (token() != FlowToken::RndOpen)
					return std::make_unique<FunctionCallExpr>((BuiltinFunction*) symbol, ExprList()/*args*/, loc);

				nextToken();
                ExprList args;
				bool rv = listExpr(args);
				consume(FlowToken::RndClose);
				if (!rv) return nullptr;
				return std::make_unique<FunctionCallExpr>((BuiltinFunction*) symbol, std::move(args), loc);
			}

			reportError("Unsupported symbol type of '%s' in expression.", name.c_str());
			return nullptr;
		}
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
			auto number = numberValue();
			nextToken();

			if (token() == FlowToken::Ident) {
				std::string sv(stringValue());
				for (size_t i = 0; units[i].ident; ++i) {
					if (sv == units[i].ident
						|| (sv[sv.size() - 1] == 's' && sv.substr(0, sv.size() - 1) == units[i].ident))
					{
						nextToken(); // UNIT
						number = number * units[i].nominator / units[i].denominator;
						loc.update(end());
						break;
					}
				}
			}
			return std::make_unique<NumberExpr>(number, loc);
		}
		case FlowToken::IP: {
			std::unique_ptr<IPAddressExpr> e = std::make_unique<IPAddressExpr>(lexer_->ipValue(), loc);
			nextToken();
			return std::move(e);
		}
		case FlowToken::Cidr: {
			std::unique_ptr<CidrExpr> e = std::make_unique<CidrExpr>(lexer_->cidr(), loc);
			nextToken();
			return std::move(e);
		}
		case FlowToken::StringType:
		case FlowToken::NumberType:
		case FlowToken::BoolType:
			return castExpr();
		case FlowToken::Begin: { // lambda-like inline function ref
			char name[64];
			static unsigned long i = 0;
            ++i;
			snprintf(name, sizeof(name), "__lambda_%lu", i);

			FlowLocation loc = location();
			auto st = std::make_unique<SymbolTable>(scope(), name);
			enter(st.get());
			std::unique_ptr<Stmt> body = compoundStmt();
			leave();

			if (!body)
				return nullptr;

			loc.update(body->location().end);

			Handler* handler = new Handler(name, std::move(st), std::move(body), loc);
			// TODO (memory leak): add handler to unit's global scope, i.e. via:
			//       - scope()->rootScope()->insert(handler);
			//       - unit_->scope()->insert(handler);
			//       to get free'd
			return std::make_unique<HandlerRefExpr>(handler, loc);
		}
		case FlowToken::RndOpen: {
			nextToken();
			std::unique_ptr<Expr> e = expr();
			consume(FlowToken::RndClose);
			e->setLocation(loc.update(end()));
			return e;
		}
		default:
			TRACE(1, "Expected primary expression. Got something... else.");
			reportUnexpectedToken();
			return nullptr;
	}
}

bool FlowParser::listExpr(ExprList& list)
{
	FlowLocation loc(location());
	std::unique_ptr<Expr> e = expr();
	if (!e)
		return false;

	list.push_back(e.release());

	while (token() == FlowToken::Comma) {
		nextToken();

		e = expr();
		if (!e)
            return false;

		list.push_back(e.release());
	}

	return true;
}

std::unique_ptr<Expr> asString(std::unique_ptr<Expr>&& expr)
{
    FlowType baseType = expr->getType();
    if (baseType == FlowType::String)
        return std::move(expr);

    Opcode opc = makeOperator(FlowToken::StringType, expr.get());
    if (opc == Opcode::EXIT)
        return nullptr; // cast error

    return std::make_unique<UnaryExpr>(opc, std::move(expr), expr->location());
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

    result = asString(std::move(result));
    if (!result) {
        reportError("Cast error in string interpolation.");
        return nullptr;
    }

	while (token() == FlowToken::InterpolatedStringFragment) {
		FlowLocation tloc = sloc.update(end());
		result = std::make_unique<BinaryExpr>(
            Opcode::SADD,
			std::move(result),
			std::make_unique<StringExpr>(stringValue(), tloc)
		);
		nextToken();

		e = expr();
		if (!e)
			return nullptr;

        e = asString(std::move(e));
        if (!e) {
            reportError("Cast error in string interpolation.");
            return nullptr;
        }

		result = std::make_unique<BinaryExpr>(Opcode::SADD, std::move(result), std::move(e));
	}

	if (!consume(FlowToken::InterpolatedStringEnd))
		return nullptr;

	if (!stringValue().empty()) {
		result = std::make_unique<BinaryExpr>(
            Opcode::SADD,
			std::move(result),
			std::make_unique<StringExpr>(stringValue(), sloc.update(end()))
		);
	}
	nextToken();
	return result;
}

// castExpr ::= 'int' '(' expr ')'
//            | 'string' '(' expr ')'
//            | 'bool' '(' expr ')'
std::unique_ptr<Expr> FlowParser::castExpr()
{
	FNTRACE();
	FlowLocation sloc(location());

	FlowToken targetTypeToken = token();
	nextToken();

	if (!consume(FlowToken::RndOpen))
		return nullptr;

	std::unique_ptr<Expr> e(expr());

	if (!consume(FlowToken::RndClose))
		return nullptr;

	if (!e)
		return nullptr;

    Opcode targetType = makeOperator(targetTypeToken, e.get());
    if (targetType == Opcode::EXIT) {
        reportError("Type cast error. No cast implementation found for requested cast from %s to %s.",
                tos(e->getType()).c_str(), targetTypeToken.c_str());
        return nullptr;
    }

    if (targetType == Opcode::NOP) {
        return e;
    }

    printf("Type cast from %s to %s: %s\n", tos(e->getType()).c_str(), targetTypeToken.c_str(), mnemonic(targetType));
	return std::make_unique<UnaryExpr>(targetType, std::move(e), sloc.update(end()));
}
// }}}
// {{{ stmt
std::unique_ptr<Stmt> FlowParser::stmt()
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
			FlowLocation sloc(location());
			nextToken();
			return std::make_unique<CompoundStmt>(sloc.update(end()));
		}
		default:
			reportError("Unexpected token '%s'. Expected a statement instead.", token().c_str());
			return nullptr;
	}
}

std::unique_ptr<Stmt> FlowParser::ifStmt()
{
	// ifStmt ::= 'if' expr ['then'] stmt ['else' stmt]
	FNTRACE();
	FlowLocation sloc(location());

	consume(FlowToken::If);
	std::unique_ptr<Expr> cond(expr());
	consumeIf(FlowToken::Then);

	std::unique_ptr<Stmt> thenStmt(stmt());
	if (!thenStmt)
		return nullptr;

	std::unique_ptr<Stmt> elseStmt;

	if (consumeIf(FlowToken::Else)) {
		elseStmt = std::move(stmt());
		if (!elseStmt) {
			return nullptr;
		}
	}

	return std::make_unique<CondStmt>(std::move(cond),
		std::move(thenStmt), std::move(elseStmt), sloc.update(end()));
}

// compoundStmt ::= '{' varDecl* stmt* '}'
std::unique_ptr<Stmt> FlowParser::compoundStmt()
{
	FNTRACE();
	FlowLocation sloc(location());
	nextToken(); // '{'

	std::unique_ptr<CompoundStmt> cs = std::make_unique<CompoundStmt>(sloc);

	while (token() == FlowToken::Var) {
		if (std::unique_ptr<Variable> var = varDecl())
			scope()->appendSymbol(std::move(var));
		else
			return nullptr;
	}

	for (;;) {
		if (consumeIf(FlowToken::End)) {
			cs->location().update(end());
			return std::unique_ptr<Stmt>(cs.release());
		}

		if (std::unique_ptr<Stmt> s = stmt())
			cs->push_back(std::move(s));
		else
			return nullptr;
	}
}

std::unique_ptr<Stmt> FlowParser::callStmt()
{
	// callStmt ::= NAME ['(' exprList ')' | exprList] (';' | LF)
	// 			 | NAME '=' expr [';' | LF]
	// NAME may be a builtin-function, builtin-handler, handler-name, or variable.

	FNTRACE();

	FlowLocation loc(location());
	std::string name = stringValue();
	nextToken(); // IDENT

	std::unique_ptr<Stmt> stmt;
	Symbol* callee = scope()->lookup(name, Lookup::All);
	if (!callee) {
        // XXX assume that given symbol is a auto forward-declared handler.
        callee = (Handler*) globalScope()->appendSymbol(std::make_unique<Handler>(name, loc));
	}

	bool callArgs = false;
	switch (callee->type()) {
		case Symbol::Variable: { // var '=' expr (';' | LF)
			if (!consume(FlowToken::Assign))
				return nullptr;

			std::unique_ptr<Expr> value = expr();
			if (!value)
				return nullptr;

			stmt = std::make_unique<AssignStmt>(static_cast<Variable*>(callee), std::move(value), loc.update(end()));
			break;
		}
		case Symbol::BuiltinHandler:
		case Symbol::BuiltinFunction:
			stmt = std::make_unique<CallStmt>(loc, (Callable*) callee);
			callArgs = true;
			break;
		case Symbol::Handler:
			stmt = std::make_unique<CallStmt>(loc, (Callable*) callee);
            break;
		default:
			break;
	}

	if (callArgs) {
		CallStmt* callStmt = static_cast<CallStmt*>(stmt.get());

		if (token() == FlowToken::RndOpen) {
			nextToken();
            ExprList args;
			bool rv = listExpr(args);
			consume(FlowToken::RndClose);
			if (!rv) return nullptr;
			callStmt->setArgs(std::move(args));
		}
		else if (lexer_->line() == loc.begin.line) {
            ExprList args;
			if (!listExpr(args))
                return nullptr;
			callStmt->setArgs(std::move(args));
		}

        // match call parameters
        FlowVM::Signature sig;
        sig.setName(name);
        sig.setReturnType(callStmt->callee()->signature().returnType()); // XXX cheetah
        std::vector<FlowType> argTypes;
        for (const auto& arg: callStmt->args()) {
            argTypes.push_back(arg->getType());
        }
        sig.setArgs(argTypes);
        if (sig != callStmt->callee()->signature()) {
            reportError("Callee parameter type signature mismatch: %s passed, but %s expected.\n", 
                    sig.to_s().c_str(), callStmt->callee()->signature().to_s().c_str());
            return nullptr;
        }
	}

	switch (token()) {
		case FlowToken::If:
		case FlowToken::Unless:
			return postscriptStmt(std::move(stmt));
		case FlowToken::Semicolon:
			// stmt ';'
			nextToken();
			stmt->location().update(end());
			return stmt;
		default:
			if (stmt->location().end.line != lexer_->line())
				return stmt;

			reportUnexpectedToken();
			return nullptr;
	}
}

std::unique_ptr<Stmt> FlowParser::postscriptStmt(std::unique_ptr<Stmt> baseStmt)
{
	FNTRACE();

	if (token() == FlowToken::Semicolon) {
		nextToken();
		return baseStmt;
	}

	if (baseStmt->location().end.line != lexer_->line())
		return baseStmt;

	FlowToken op = token();
	switch (op) {
		case FlowToken::If:
		case FlowToken::Unless:
			break;
		default:
			return baseStmt;
	}

	// STMT ['if' EXPR] ';'
	// STMT ['unless' EXPR] ';'

	FlowLocation sloc = location();

	nextToken(); // 'if' | 'unless'

	std::unique_ptr<Expr> condExpr = expr();
	if (!condExpr)
		return nullptr;

	consumeIf(FlowToken::Semicolon);

	if (op == FlowToken::Unless) {
        auto opc = makeOperator(FlowToken::Not, condExpr.get());
        if (opc == Opcode::EXIT) {
            reportError("Type cast error. No cast implementation found for requested cast from %s to %s.",
                    tos(condExpr->getType()).c_str(), "bool");
            return nullptr;
        }

		condExpr = std::make_unique<UnaryExpr>(opc, std::move(condExpr), sloc);
    }

	return std::make_unique<CondStmt>(std::move(condExpr), std::move(baseStmt), nullptr, sloc.update(end()));
}
// }}}

} // namespace x0
