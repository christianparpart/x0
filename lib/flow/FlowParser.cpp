#include <x0/flow/vm/NativeCallback.h>
#include <x0/flow/FlowParser.h>
#include <x0/flow/FlowLexer.h>
#include <x0/flow/vm/Runtime.h>
#include <x0/flow/AST.h>
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
    // (num, num)       + - * / % ** << >> & | ^ and or xor == != <= >= < >
    // (string, string) + == != <= >= < > =^ =$ in
    // (string, regex)  =~
    // (ip, ip)         == !=
    // (ip, cidr)       in
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
            {FlowToken::Pow, Opcode::NPOW},
            {FlowToken::Shl, Opcode::NSHL},
            {FlowToken::Shr, Opcode::NSHR},
            {FlowToken::BitAnd, Opcode::NAND},
            {FlowToken::BitOr, Opcode::NOR},
            {FlowToken::BitXor, Opcode::NXOR},
            {FlowToken::Equal, Opcode::NCMPEQ},
            {FlowToken::UnEqual, Opcode::NCMPNE},
            {FlowToken::LessOrEqual, Opcode::NCMPLT},
            {FlowToken::GreaterOrEqual, Opcode::NCMPGT},
            {FlowToken::Less, Opcode::NCMPLE},
            {FlowToken::Greater, Opcode::NCMPGE},
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
            {FlowToken::Equal, Opcode::PCMPEQ},
            {FlowToken::UnEqual, Opcode::PCMPNE},
        }},
        {OpSig::IpCidr, {
            {FlowToken::In, Opcode::PINCIDR},
        }},
        {OpSig::CidrCidr, {
            {FlowToken::Equal, Opcode::NOP}, // TODO
            {FlowToken::UnEqual, Opcode::NOP}, // TODO
            {FlowToken::In, Opcode::NOP}, // TODO
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
        {FlowType::IPAddress, {
            {FlowToken::StringType, Opcode::P2S},
            {FlowToken::IPType, Opcode::NOP},
        }},
        {FlowType::Cidr, {
            {FlowToken::StringType, Opcode::C2S},
            {FlowToken::CidrType, Opcode::NOP},
        }},
#if 0 // TODO
        {FlowType::RegExp, {
            {FlowToken::StringType, Opcode::REGEXP2S},
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
            //printf("%s\n", builtin->signature().to_s().c_str());

			if (builtin->isHandler()) {
				createSymbol<BuiltinHandler>(builtin->signature());
            } else {
				createSymbol<BuiltinFunction>(builtin->signature());
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
			reportError("Redeclaring handler \"%s\"", handler->name().c_str());
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
		{ FlowToken::Plus, 3 },
		{ FlowToken::Minus, 3 },

		// mul expr
		{ FlowToken::Mul, 4 },
		{ FlowToken::Div, 4 },
		{ FlowToken::Mod, 4 },
		{ FlowToken::Shl, 4 },
		{ FlowToken::Shr, 4 },

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

std::unique_ptr<Expr> FlowParser::addExpr()
{
	std::unique_ptr<Expr> lhs = powExpr();
	if (!lhs)
		return nullptr;

	return rhsExpr(std::move(lhs), 2 /* REL_OP precedence */ );
}

/**
 * Parses a binary expression.
 *
 * @param lhs            left hand side of the binary expression
 * @param lastPrecedence precedence of \p lhs.
 *
 * @return nullptr on failure, the resulting expression AST otherwise.
 */
std::unique_ptr<Expr> FlowParser::rhsExpr(std::unique_ptr<Expr> lhs, int lastPrecedence)
{
    // rhsExpr ::= (BIN_OP primaryExpr)*

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

// primaryExpr ::= literalExpr
//               | variable
//               | function '(' paramList ')'
//               | '(' expr ')'
std::unique_ptr<Expr> FlowParser::primaryExpr()
{
	FNTRACE();

	switch (token()) {
        case FlowToken::String:
        case FlowToken::RawString:
        case FlowToken::Number:
        case FlowToken::Boolean:
        case FlowToken::IP:
        case FlowToken::Cidr:
        case FlowToken::RegExp:
            return literalExpr();
        case FlowToken::StringType:
        case FlowToken::NumberType:
        case FlowToken::BoolType:
            return castExpr();
        case FlowToken::InterpolatedStringFragment:
            return interpolatedStr();
		case FlowToken::Ident: {
			FlowLocation loc = location();
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
					return std::make_unique<FunctionCall>(loc, (BuiltinFunction*) symbol);

                consume(FlowToken::RndOpen);
                std::unique_ptr<ParamList> args;
                if (token() != FlowToken::RndClose) {
                    args = paramList();
                    if (!args) {
                        return nullptr;
                    }
                }
				consume(FlowToken::RndClose);
                return std::make_unique<FunctionCall>(loc, (BuiltinFunction*) symbol, std::move(*args));
			}

			reportError("Unsupported symbol type of \"%s\" in expression.", name.c_str());
			return nullptr;
		}
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
			FlowLocation loc = location();
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

std::unique_ptr<Expr> FlowParser::literalExpr()
{
    // literalExpr  ::= NUMBER [UNIT]
    //                | BOOL
    //                | STRING
    //                | IP_ADDR
    //                | IP_CIDR
    //                | REGEXP

    static struct {
        const char* ident;
        long long nominator;
        long long denominator;
    } units[] = {
        { "byte",  1, 1 },
        { "kbyte", 1024llu, 1 },
        { "mbyte", 1024llu * 1024, 1 },
        { "gbyte", 1024llu * 1024 * 1024, 1 },
        { "tbyte", 1024llu * 1024 * 1024 * 1024, 1 },
        { "bit",   1, 8 },
        { "kbit",  1024llu, 8 },
        { "mbit",  1024llu * 1024, 8 },
        { "gbit",  1024llu * 1024 * 1024, 8 },
        { "tbit",  1024llu * 1024 * 1024 * 1024, 8 },
        { "sec",   1, 1 },
        { "min",   60llu, 1 },
        { "hour",  60llu * 60, 1 },
        { "day",   60llu * 60 * 24, 1 },
        { "week",  60llu * 60 * 24 * 7, 1 },
        { "month", 60llu * 60 * 24 * 30, 1 },
        { "year",  60llu * 60 * 24 * 365, 1 },
        { nullptr, 1, 1 }
    };

    FlowLocation loc(location());

    switch (token()) {
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
        case FlowToken::Boolean: {
            std::unique_ptr<BoolExpr> e = std::make_unique<BoolExpr>(booleanValue(), loc);
            nextToken();
            return std::move(e);
        }
        case FlowToken::String:
        case FlowToken::RawString: {
            std::unique_ptr<StringExpr> e = std::make_unique<StringExpr>(stringValue(), loc);
            nextToken();
            return std::move(e);
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
        case FlowToken::RegExp: {
            std::unique_ptr<RegExpExpr> e = std::make_unique<RegExpExpr>(RegExp(stringValue()), loc);
            nextToken();
            return std::move(e);
        }
        default:
            reportError("Expected literal expression, but got %s.", token().c_str());
            return nullptr;
    }
}

std::unique_ptr<ParamList> FlowParser::paramList()
{
    // paramList       ::= namedExpr *(',' namedExpr)
    //                   | expr *(',' expr)

    if (token() == FlowToken::NamedParam) {
        std::unique_ptr<ParamList> args(new ParamList(true));
        std::string name;
        std::unique_ptr<Expr> e = namedExpr(&name);
        if (!e)
            return nullptr;

        args->push_back(name, std::move(e));

        while (token() == FlowToken::Comma) {
            nextToken();

            if (token() == FlowToken::RndClose)
                break;

            e = namedExpr(&name);
            if (!e)
                return nullptr;

            args->push_back(name, std::move(e));
        }
        return args;
    } else {
        // unnamed param list
        std::unique_ptr<ParamList> args(new ParamList(false));

        std::unique_ptr<Expr> e = expr();
        if (!e)
            return nullptr;

        args->push_back(std::move(e));

        while (token() == FlowToken::Comma) {
            nextToken();

            if (token() == FlowToken::RndClose)
                break;

            e = expr();
            if (!e)
                return nullptr;

            args->push_back(std::move(e));
        }
        return args;
    }
}

std::unique_ptr<Expr> FlowParser::namedExpr(std::string* name)
{
    // namedExpr       ::= NAMED_PARAM expr

    // FIXME: wtf? what way around?

    *name = stringValue();
    if (!consume(FlowToken::NamedParam))
        return nullptr;

    return expr();
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
        case FlowToken::Match:
            return matchStmt();
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
			reportError("Unexpected token \"%s\". Expected a statement instead.", token().c_str());
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

std::unique_ptr<Stmt> FlowParser::matchStmt()
{
	FNTRACE();

    // matchStmt       ::= 'match' expr [MATCH_OP] '{' *matchCase ['else' stmt] '}'
    // matchCase       ::= 'on' literalExpr stmt
    // MATCH_OP        ::= '==' | '=^' | '=$' | '=~'

	FlowLocation sloc(location());

    if (!consume(FlowToken::Match))
        return nullptr;

    auto cond = addExpr();
    if (!cond)
        return nullptr;

    FlowType matchType = cond->getType();

    if (matchType != FlowType::String) {
        reportError("Expected match condition type <%s>, found \"%s\" instead.",
                tos(FlowType::String).c_str(), tos(matchType).c_str());
        return nullptr;
    }

    // [MATCH_OP]
    FlowVM::MatchClass op;
    if (FlowTokenTraits::isOperator(token())) {
        switch (token()) {
            case FlowToken::Equal:
                op = FlowVM::MatchClass::Same;
                break;
            case FlowToken::PrefixMatch:
                op = FlowVM::MatchClass::Head;
                break;
            case FlowToken::SuffixMatch:
                op = FlowVM::MatchClass::Tail;
                break;
            case FlowToken::RegexMatch:
                op = FlowVM::MatchClass::RegExp;
                break;
            default:
                reportError("Expected match oeprator, found \"%s\" instead.", token().c_str());
                return nullptr;
        }
        nextToken();
    } else {
        op = FlowVM::MatchClass::Same;
    }

    if (op == FlowVM::MatchClass::RegExp)
        matchType = FlowType::RegExp;

    // '{'
    if (!consume(FlowToken::Begin))
        return nullptr;

    // *('on' expr stmt)
    MatchStmt::CaseList cases;
    do {
        if (!consume(FlowToken::On)) {
            return nullptr;
        }

        MatchCase one;
        one.first = literalExpr();
        if (!one.first)
            return nullptr;

        FlowType caseType = one.first->getType();
        if (matchType != caseType) {
            reportError("Type mismatch in match-on statement. Expected <%s> but got <%s>.",
                    tos(matchType).c_str(), tos(caseType).c_str());
            return nullptr;
        }

        one.second = stmt();
        if (!one.second)
            return nullptr;

        cases.push_back(std::move(one));
    }
    while (token() == FlowToken::On);

    // ['else' stmt]
    std::unique_ptr<Stmt> elseStmt;
    if (consumeIf(FlowToken::Else)) {
        elseStmt = stmt();
        if (!elseStmt) {
            return nullptr;
        }
    }

    // '}'
    if (!consume(FlowToken::End))
        return nullptr;

    return std::make_unique<MatchStmt>(sloc.update(end()), std::move(cond), op, std::move(cases), std::move(elseStmt));
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
	// callStmt ::= NAME ['(' paramList ')' | paramList] (';' | LF)
	// 			 | NAME '=' expr [';' | LF]
	// NAME may be a builtin-function, builtin-handler, handler-name, or variable.
    //
    // namedArg ::= NAME ':' expr

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

	switch (callee->type()) {
		case Symbol::Variable: { // var '=' expr (';' | LF)
			if (!consume(FlowToken::Assign))
				return nullptr;

			std::unique_ptr<Expr> value = expr();
			if (!value)
				return nullptr;

            Variable* var = static_cast<Variable*>(callee);
            FlowType leftType = var->initializer()->getType();
            FlowType rightType = value->getType();
            if (leftType != rightType) {
                reportError("Type mismatch in assignment. Expected <%s> but got <%s>.",
                        tos(leftType).c_str(), tos(rightType).c_str());
                return nullptr;
            }

            stmt = std::make_unique<AssignStmt>(var, std::move(value), loc.update(end()));
			break;
		}
        case Symbol::BuiltinHandler: {
            HandlerCall* call = new HandlerCall(loc, (BuiltinHandler*) callee);
            if (!callArgs(loc, call->callee(), call->args()))
                return nullptr;
            stmt.reset(call);
            break;
        }
        case Symbol::BuiltinFunction: {
            std::unique_ptr<FunctionCall> call = std::make_unique<FunctionCall>(loc, (BuiltinFunction*) callee);
            if (!callArgs(loc, call->callee(), call->args()))
                return nullptr;
            stmt = std::make_unique<ExprStmt>(std::move(call));
            break;
        }
		case Symbol::Handler:
			stmt = std::make_unique<HandlerCall>(loc, (Callable*) callee);
            break;
		default:
			break;
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

/**
 * Parses function/handler call parameters and performs semantic checks.
 *
 * @param loc    Source location of the call.
 * @param callee Pointer to the callee's AST node.
 * @param args   Output argument list to store parsed parameters into.
 *
 * @retval false Parsing or sema checks failed.
 * @retval true  Parsing and sema checks succeed.
 */
bool FlowParser::callArgs(const FlowLocation& loc, Callable* callee, ParamList& args)
{
    // callArgs ::= '(' paramList ')'
    //            | paramList           /* if starting on same line */

    if (token() == FlowToken::RndOpen) {
        nextToken();
        if (token() != FlowToken::RndClose) {
            auto ra = paramList();
            if (!ra) {
                return false;
            }
            args = std::move(*ra);
        }
        consume(FlowToken::RndClose);
    } else if (lexer_->line() == loc.begin.line) {
        auto ra = paramList();
        if (!ra) {
            return false;
        }
        args = std::move(*ra);
    }

    if (args.isNamed()) {
        return verifyParamsNamed(callee, args);
    } else {
        return verifyParamsPositional(callee, args);
    }
}

/**
 * Verify <b>named</b> call-parameters, possibly auto-fill missing defaults and reorder them if needed.
 *
 * @param callee  callee to verify
 * @param args    callees argument list
 *
 * @retval true   Verification succeed.
 * @retval false  Verification failed.
 */
bool FlowParser::verifyParamsNamed(const Callable* callee, ParamList& args)
{
    // auto-fill missing for defaulted parameters
    const FlowVM::NativeCallback* native = runtime_->find(callee->signature());
    if (!native->isNamed()) {
        reportError("Callee \"%s\" invoked with named parameters, but no names provided by runtime.",
                callee->name().c_str());
        return false;
    }

    int argc = native->signature().args().size();
    for (int i = 0; i != argc; ++i) {
        const auto& name = native->getNameAt(i);
        if (!args.contains(name)) {
            const void* defaultValue = native->getDefaultAt(i);
            if (!defaultValue) {
                reportError("Callee \"%s\" invoked without required named parameter \"%s\".",
                        callee->name().c_str(), name.c_str());
                return false;
            }
            FlowType type = native->signature().args()[i];
            switch (type) {
                case FlowType::Boolean:
                    args.push_back(name, std::make_unique<BoolExpr>((bool) defaultValue));
                    break;
                case FlowType::Number:
                    args.push_back(name, std::make_unique<NumberExpr>((FlowNumber) defaultValue));
                    break;
                case FlowType::String:
                    args.push_back(name, std::make_unique<StringExpr>(*(std::string*) defaultValue));
                    break;
                case FlowType::IPAddress:
                    args.push_back(name, std::make_unique<IPAddressExpr>(*(IPAddress*) defaultValue));
                    break;
                case FlowType::Cidr:
                    args.push_back(name, std::make_unique<CidrExpr>(*(Cidr*) defaultValue));
                    break;
                default:
                    reportError("Cannot complete named paramter \"%s\" in callee \"%s\". Unsupported type <%s>.",
                            name.c_str(), callee->name().c_str(), tos(type).c_str());
                    return false;
            }
        }
    }

    // reorder args (and detect superfluous args)
    std::vector<std::string> superfluous;
    args.reorder(native, &superfluous);
    if (!superfluous.empty()) {
        std::string t;
        for (const auto& s: superfluous) {
            if (!t.empty()) t += ", ";
            t += "\"";
            t += s;
            t += "\"";
        }
        reportError("Superfluous arguments passed to callee \"%s\": %s.",
            callee->name().c_str(), t.c_str());
        return false;
    }

    return true;
}

bool FlowParser::verifyParamsPositional(const Callable* callee, const ParamList& args)
{
    FlowVM::Signature sig;
    sig.setName(callee->name());
    sig.setReturnType(callee->signature().returnType()); // XXX cheetah
    std::vector<FlowType> argTypes;
    for (const auto& arg: args.values()) {
        argTypes.push_back(arg->getType());
    }
    sig.setArgs(argTypes);

    if (sig != callee->signature()) {
        reportError("Callee parameter type signature mismatch: %s passed, but %s expected.", 
                sig.to_s().c_str(), callee->signature().to_s().c_str());
        return false;
    }

    return true;
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
