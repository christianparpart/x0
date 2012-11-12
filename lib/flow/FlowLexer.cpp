/* <flow/FlowLexer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2010-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/flow/FlowLexer.h>
#include <cstring>
#include <cstdio>

namespace x0 {

static std::string unescape(const std::string& value)
{
	std::string result;

	for (auto i = value.begin(), e = value.end(); i != e; ++i) {
		if (*i != '\\') {
			result += *i;
		} else if (++i != e) {
			switch (*i) {
				case '\\':
					result += '\\';
					break;
				case 'r':
					result += '\r';
					break;
				case 'n':
					result += '\n';
					break;
				case 't':
					result += '\t';
					break;
				default:
					result += *i;
			}
		}
	}

	return result;
}

std::string SourceLocation::dump(const std::string& prefix) const
{
	char buf[4096];
	std::size_t n = snprintf(buf, sizeof(buf), "%s: { %ld:%ld - %ld:%ld }",
		!prefix.empty() ? prefix.c_str() : "location",
		begin.line, begin.column,
		end.line, end.column);
	return std::string(buf, n);
}

FlowLexer::FlowLexer() :
	filename_(),
	stream_(NULL),
	lastPos_(),
	currentPos_(),
	location_(),
	lastLocation_(),

	currentChar_(EOF),
	ipv6HexDigits_(0),
	numberValue_(0),
	ipValue_(),
	stringValue_(),
	token_(FlowToken::Unknown),
	interpolationDepth_(0)
{
}

FlowLexer::~FlowLexer()
{
}

bool FlowLexer::initialize(std::istream *input, const std::string& name)
{
	stream_ = input;
	filename_ = name;

	if (stream_ == NULL)
		return false;

	lastPos_.set(1, 1, 0);
	currentPos_.set(1, 0, 0);
	currentChar_ = '\0';

	location_.fileName = filename_;
	location_.begin.set(1, 1, 0);
	location_.end.set(1, 0, 0);
	content_.clear();

	nextChar();
	nextToken();

	return true;
}

size_t FlowLexer::line() const
{
	return currentPos_.line;
}

size_t FlowLexer::column() const
{
	return currentPos_.column;
}

bool FlowLexer::eof() const
{
	return !stream_ || stream_->eof();
}

std::string FlowLexer::stringValue() const
{
	return stringValue_;
}

long long FlowLexer::numberValue() const
{
	return numberValue_;
}

const IPAddress& FlowLexer::ipValue() const
{
	return ipValue_;
}

bool FlowLexer::booleanValue() const
{
	return numberValue_ != 0;
}

int FlowLexer::currentChar() const
{
	return currentChar_;
}

int FlowLexer::peekChar()
{
	return stream_->peek();
}

int FlowLexer::nextChar()
{
	if (currentChar_ == EOF)
		return currentChar_;

	int ch = stream_->get();

	if (ch != EOF) {
		lastPos_ = currentPos_;

		if (currentChar_ == '\n') {
			currentPos_.column = 0;
			++currentPos_.line;
		}
		++currentPos_.column;
		++currentPos_.offset;
		content_ += static_cast<char>(currentChar_);
	}

	return currentChar_ = ch;
}

bool FlowLexer::advanceUntil(char value)
{
	while (currentChar() != value && !eof())
		nextChar();

	return !eof();
}

FlowToken FlowLexer::token() const
{
	return token_;
}

FlowToken FlowLexer::nextToken()
{
	bool expectsValue = token() == FlowToken::Ident || FlowTokenTraits::isOperator(token());
	//printf("FlowLexer.nextToken(): current:%s, EV:%d\n", tokenToString(token()).c_str(), expectsValue);

	if (consumeSpace())
		return token_ = FlowToken::Eof;

	lastLocation_ = location_;
	location_.begin = currentPos_;
	content_.clear();

	switch (currentChar_) {
	case EOF: // (-1)
		return token_ = FlowToken::Eof;
	case '=':
		switch (nextChar()) {
		case '=':
			nextChar();
			return token_ = FlowToken::Equal;
		case '^':
			nextChar();
			return token_ = FlowToken::PrefixMatch;
		case '$':
			nextChar();
			return token_ = FlowToken::SuffixMatch;
		case '~':
			nextChar();
			return token_ = FlowToken::RegexMatch;
		case '>':
			nextChar();
			return token_ = FlowToken::HashRocket;
		default:
			return token_ = FlowToken::Assign;
		}
	case '<':
		switch (nextChar()) {
			case '<':
				nextChar();
				return token_ = FlowToken::Shl;
			case '=':
				nextChar();
				return token_ = FlowToken::LessOrEqual;
			default:
				return token_ = FlowToken::Less;
		}
	case '>':
		switch (nextChar()) {
			case '>':
				nextChar();
				return token_ = FlowToken::Shr;
			case '=':
				nextChar();
				return token_ = FlowToken::GreaterOrEqual;
			default:
				return token_ = FlowToken::Greater;
		}
	case '|':
		switch (nextChar()) {
			case '|':
				nextChar();
				return token_ = FlowToken::Or;
			case '=':
				nextChar();
				return token_ = FlowToken::OrAssign;
			default:
				return token_ = FlowToken::BitOr;
		}
	case '&':
		switch (nextChar()) {
			case '&':
				nextChar();
				return token_ = FlowToken::And;
			case '=':
				nextChar();
				return token_ = FlowToken::AndAssign;
			default:
				return token_ = FlowToken::BitAnd;
		}
	case '.':
		if (nextChar() == '.') {
			if (nextChar() == '.') {
				nextChar();
				return token_ = FlowToken::Ellipsis;
			}
			return token_ = FlowToken::DblPeriod;
		}
		return token_ = FlowToken::Period;
	case ':':
		if (peekChar() == ':') {
			stringValue_.clear();
			return continueParseIPv6(false);
		} else {
			nextChar(); // skip the invalid char
			return token_ = FlowToken::Unknown;
		}
	case ';':
		nextChar();
		return token_ = FlowToken::Semicolon;
	case ',':
		nextChar();
		return token_ = FlowToken::Comma;
	case '{':
		nextChar();
		return token_ = FlowToken::Begin;
	case '}':
		if (interpolationDepth_) {
			return token_ = parseInterpolationFragment(false);
		} else {
			nextChar();
			return token_ = FlowToken::End;
		}
	case '(':
		nextChar();
		return token_ = FlowToken::RndOpen;
	case ')':
		nextChar();
		return token_ = FlowToken::RndClose;
	case '[':
		nextChar();
		return token_ = FlowToken::BrOpen;
	case ']':
		nextChar();
		return token_ = FlowToken::BrClose;
	case '+':
		nextChar();
		return token_ = FlowToken::Plus;
	case '-':
		nextChar();
		return token_ = FlowToken::Minus;
	case '*':
		switch (nextChar()) {
			case '*':
				nextToken();
				return token_ = FlowToken::Pow;
			default:
				return token_ = FlowToken::Mul;
		}
	case '/':
		if (expectsValue)
			return parseString('/', FlowToken::RegExp);

		nextChar();
		return token_ = FlowToken::Div;
	case '%':
		nextChar();
		return token_ = FlowToken::Mod;
	case '!':
		switch (nextChar()) {
			case '=':
				nextChar();
				return token_ = FlowToken::UnEqual;
			default:
				return token_ = FlowToken::Not;
		}
	case '\'':
		return parseString(true);
	case '"':
		++interpolationDepth_;
		token_ = parseInterpolationFragment(true);
		return token_;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		return parseNumber();
	default:
		if (std::isalpha(currentChar()) || currentChar() == '_')
			return parseIdent();

		if (std::isprint(currentChar()))
			printf("lexer: unknown char %c (0x%02X)\n", currentChar(), currentChar());
		else
			printf("lexer: unknown char %u (0x%02X)\n", currentChar() & 0xFF, currentChar() & 0xFF);

		nextChar();
		return token_ = FlowToken::Unknown;
	}
}

FlowToken FlowLexer::parseString(char delimiter, FlowToken result)
{
	int delim = currentChar();
	int last = -1;

	nextChar(); // skip left delimiter
	stringValue_.clear();

	while (!eof() && (currentChar() != delim || (last == '\\'))) {
		stringValue_ += static_cast<char>(currentChar());

		last = currentChar();
		nextChar();
	}

	if (currentChar() == delim) {
		nextChar();

		return token_ = result;
	}

	return token_ = FlowToken::Unknown;
}

SourceLocation FlowLexer::location()
{
	location_.end = lastPos_;
	return location_;
}

const SourceLocation& FlowLexer::lastLocation() const
{
	return lastLocation_;
}

std::string FlowLexer::locationContent()
{
	return content_;
}

bool FlowLexer::consume(char c)
{
	bool result = currentChar() == c;
	nextChar();
	return result;
}

/**
 * \retval true abort tokenizing in caller
 * \retval false continue tokenizing in caller
 */
bool FlowLexer::consumeSpace()
{
	// skip spaces
	for (;; nextChar()) {
		if (eof())
			return true;

		if (std::isspace(currentChar_))
			continue;

		if (std::isprint(currentChar_))
			break;

		// TODO proper error reporting through API callback
		std::fprintf(stderr, "%s[%04ld:%02ld]: invalid byte %d (0x%02X)\n",
				location_.fileName.c_str(), currentPos_.line, currentPos_.column,
				currentChar() & 0xFF, currentChar() & 0xFF);
	}

	if (eof())
		return true;

	if (currentChar() == '#') {
		// skip chars until EOL
		for (;;) {
			if (eof()) {
				token_ = FlowToken::Eof;
				return true;
			}

			if (currentChar() == '\n') {
				nextChar();
				return consumeSpace();
			}

			nextChar();
		}
	}

	if (currentChar() == '/' && peekChar() == '*') { // "/*" ... "*/"
		// parse multiline comment
		nextChar();

		for (;;) {
			if (eof()) {
				token_ = FlowToken::Eof;
				// reportError(Error::UnexpectedEof);
				return true;
			}

			if (currentChar() == '*' && peekChar() == '/') {
				nextChar(); // skip '*'
				nextChar(); // skip '/'
				break;
			}

			nextChar();
		}

		return consumeSpace();
	}

	return false;
}

FlowToken FlowLexer::parseString(bool raw)
{
	FlowToken result = parseString(raw ? '\'' : '"', FlowToken::String);

	if (result == FlowToken::String && raw)
		stringValue_ = unescape(stringValue_);

	return result;
}

FlowToken FlowLexer::parseInterpolationFragment(bool start)
{
	int last = -1;
	stringValue_.clear();

	// skip either '"' or '}' depending on your we entered
	nextChar();

	for (;;) {
		if (eof() || (currentChar() == '"' && last != '\\'))
			break;

		if (currentChar() == '\\') {
			nextChar();

			if (eof()) {
				break;
			}
		} else if (currentChar() == '#') {
			nextChar();
			if (currentChar() == '{') {
				nextChar();
				return token_ = FlowToken::InterpolatedStringFragment;
			} else {
				stringValue_ += '#';
			}
		}

		stringValue_ += static_cast<char>(currentChar());
		last = currentChar();
		nextChar();
	}

	if (currentChar() == '"') {
		nextChar();
		--interpolationDepth_;
		return token_ = start ? FlowToken::String : FlowToken::InterpolatedStringEnd;
	} else if (eof()) {
		return token_ = FlowToken::Eof;
	} else {
		return token_ = FlowToken::Unknown;
	}
}

FlowToken FlowLexer::parseNumber()
{
	stringValue_.clear();
	numberValue_ = 0;

	while (std::isdigit(currentChar())) {
		numberValue_ *= 10;
		numberValue_ += currentChar() - '0';
		stringValue_ += static_cast<char>(currentChar());
		nextChar();
	}

	// ipv6HexDigit4 *(':' ipv6HexDigit4) ['::' [ipv6HexSeq]]
	if (stringValue_.size() <= 4 && currentChar() == ':')
		return continueParseIPv6(true);

	if (stringValue_.size() < 4 && isHexChar())
		return continueParseIPv6(false);

	if (currentChar() != '.')
		return token_ = FlowToken::Number;

	// 2nd IP component
	stringValue_ += '.';
	nextChar();
	while (std::isdigit(currentChar())) {
		stringValue_ += static_cast<char>(currentChar());
		nextChar();
	}

	// 3rd IP component
	if (!consume('.'))
		return token_ = FlowToken::Unknown;

	stringValue_ += '.';
	while (std::isdigit(currentChar())) {
		stringValue_ += static_cast<char>(currentChar());
		nextChar();
	}

	// 4th IP component
	if (!consume('.'))
		return token_ = FlowToken::Unknown;

	stringValue_ += '.';
	while (std::isdigit(currentChar())) {
		stringValue_ += static_cast<char>(currentChar());
		nextChar();
	}

	ipValue_.set(stringValue_.c_str(), IPAddress::V4);

	return token_ = FlowToken::IP;
}

FlowToken FlowLexer::parseIdent()
{
	stringValue_.clear();
	stringValue_ += static_cast<char>(currentChar());
	bool isHex = isHexChar();

	nextChar();

	while (std::isalnum(currentChar()) || currentChar() == '_' || currentChar() == '.') {
		stringValue_ += static_cast<char>(currentChar());
		if (!isHexChar())
			isHex = false;

		nextChar();
	}

	// ipv6HexDigit4 *(':' ipv6HexDigit4) ['::' [ipv6HexSeq]]
	if (stringValue_.size() <= 4 && isHex && currentChar() == ':')
		return continueParseIPv6(true);

	if (stringValue_.size() < 4 && isHex && isHexChar())
		return continueParseIPv6(false);

	static struct {
		const char *symbol;
		FlowToken token;
	} keywords[] = {
		{ "in", FlowToken::In },
		{ "var", FlowToken::Var },
		{ "on", FlowToken::On },
		{ "do", FlowToken::Do },
		{ "if", FlowToken::If },
		{ "then", FlowToken::Then },
		{ "else", FlowToken::Else },
		{ "unless", FlowToken::Unless },
		{ "import", FlowToken::Import },
		{ "from", FlowToken::From },
		{ "handler", FlowToken::Handler },
		{ "and", FlowToken::And },
		{ "or", FlowToken::Or },
		{ "xor", FlowToken::Xor },
		{ "not", FlowToken::Not },

		{ "bool", FlowToken::BoolType },
		{ "int", FlowToken::IntType },
		{ "string", FlowToken::StringType },

		{ 0, FlowToken::Unknown }
	};

	for (auto i = keywords; i->symbol; ++i)
		if (strcmp(i->symbol, stringValue_.c_str()) == 0)
			return token_ = i->token;

	if (stringValue_ == "true" || stringValue_ == "yes") {
		numberValue_ = 1;
		return token_ = FlowToken::Boolean;
	}

	if (stringValue_ == "false" || stringValue_ == "no") {
		numberValue_ = 0;
		return token_ = FlowToken::Boolean;
	}

	return token_ = FlowToken::Ident;
}

std::string FlowLexer::tokenString() const
{
	switch (token()) {
		case FlowToken::Ident:
			return std::string("ident:") + stringValue_;
		case FlowToken::IP:
			return ipValue_.str();
		case FlowToken::RegExp:
			return "/" + stringValue_ + "/";
		case FlowToken::RawString:
		case FlowToken::String:
			return "string:'" + stringValue_ + "'";
		case FlowToken::Number: {
			char buf[64];
			snprintf(buf, sizeof(buf), "%lld", numberValue_);
			return buf;
		}
		case FlowToken::Boolean:
			return booleanValue() ? "true" : "false";
		default:
			return tokenToString(token());
	}
}

std::string FlowLexer::tokenToString(FlowToken value) const
{
	return value.c_str();
}

std::string FlowLexer::dump() const
{
	char buf[4096];
	std::size_t n = snprintf(buf, sizeof(buf), "[%04ld:%02ld] %3d %s",
			line(), column(),
			static_cast<int>(token()), tokenString().c_str());

	return std::string(buf, n);
}

bool FlowLexer::isHexChar() const
{
	return (currentChar_ >= '0' && currentChar_ <= '9')
		|| (currentChar_ >= 'a' && currentChar_ <= 'f')
		|| (currentChar_ >= 'A' && currentChar_ <= 'F');
}

// {{{ IPv6 address parser

// IPv6_HexPart [':' IPv4_Addr]
FlowToken FlowLexer::parseIPv6()
{
	return token_ = FlowToken::Unknown;
}

// IPv6_HexPart ::= IPv6_HexSeq                        # (1)
//                | IPv6_HexSeq "::" [IPv6_HexSeq]     # (2)
//                            | "::" [IPv6_HexSeq]     # (3)
//
bool FlowLexer::ipv6HexPart()
{
	bool rv;

	if (currentChar() == ':' && peekChar() == ':') { // (3)
		stringValue_ = "::";
		nextChar(); // skip ':'
		nextChar(); // skip ':'
		rv = isHexChar() ? ipv6HexSeq() : true;
	} else if (!!(rv = ipv6HexSeq())) {
		if (currentChar() == ':' && peekChar() == ':') { // (2)
			stringValue_ += "::";
			nextChar(); // skip ':'
			nextChar(); // skip ':'
			rv = isHexChar() ? ipv6HexSeq() : true;
		}
	}

	if (std::isalnum(currentChar_) || currentChar_ == ':')
		rv = false;

	return rv;
}

// 1*4HEXDIGIT *(':' 1*4HEXDIGIT)
bool FlowLexer::ipv6HexSeq()
{
	if (!ipv6HexDigit4())
		return false;

	while (currentChar() == ':' && peekChar() != ':') {
		stringValue_ += ':';
		nextChar();

		if (!ipv6HexDigit4())
			return false;
	}

	return true;
}

// 1*4HEXDIGIT
bool FlowLexer::ipv6HexDigit4()
{
	size_t i = ipv6HexDigits_;

	while (isHexChar()) {
		stringValue_ += currentChar();
		nextChar();
		++i;
	}

	ipv6HexDigits_ = 0;

	return i >= 1 && i <= 4;
}

// ipv6HexDigit4 *(':' ipv6HexDigit4) ['::' [ipv6HexSeq]]
// where the first component, ipv6HexDigit4 is already parsed
FlowToken FlowLexer::continueParseIPv6(bool firstComplete)
{
	bool rv = true;
	if (firstComplete) {
		while (currentChar() == ':' && peekChar() != ':') {
			stringValue_ += ':';
			nextChar();

			if (!ipv6HexDigit4())
				return false;
		}

		if (currentChar() == ':' && peekChar() == ':') {
			stringValue_ += "::";
			nextChar();
			nextChar();
			rv = isHexChar() ? ipv6HexSeq() : true;
		}
	} else {
		ipv6HexDigits_ = stringValue_.size();
		rv = ipv6HexPart();
	}

	// parse embedded IPv4 remainer
	while (currentChar_ == '.' && std::isdigit(peekChar())) {
		stringValue_ += '.';
		nextChar();

		while (std::isdigit(currentChar_)) {
			stringValue_ += static_cast<char>(currentChar_);
			nextChar();
		}
	}

	if (rv && ipValue_.set(stringValue_.c_str(), IPAddress::V6))
		return token_ = FlowToken::IP;
	else
		return token_ = FlowToken::Unknown;
}
// }}}

} // namespace x0
