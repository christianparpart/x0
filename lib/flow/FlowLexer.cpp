/* <flow/FlowLexer.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
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

void SourceLocation::dump(const std::string& prefix) const
{
	printf("%s: { %ld:%ld - %ld:%ld }\n",
			!prefix.empty() ? prefix.c_str() : "location",
			begin.line, begin.column,
			end.line, end.column);
}

FlowLexer::FlowLexer() :
	filename_(),
	stream_(NULL),
	lastPos_(),
	currentPos_(),
	location_(),
	lastLocation_(),

	currentChar_(EOF),
	numberValue_(0),
	ipValue_(),
	stringValue_(),
	token_(FlowToken::Unknown)
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
	int ch = stream_->get();

	if (ch != EOF)
	{
		lastPos_ = currentPos_;

		if (currentChar_ != EOF) {
			content_ += (char)currentChar_;
			++currentPos_.offset;
		}

		if (currentChar_ == '\n') {
			++currentPos_.line;
			currentPos_.column = 1;
		} else {
			++currentPos_.column;
		}
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
			return token_ = FlowToken::KeyAssign;
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
		if (nextChar() == '.')
		{
			if (nextChar() == '.') {
				nextChar();
				return token_ = FlowToken::Ellipsis;
			}
			return token_ = FlowToken::DblPeriod;
		}
		return token_ = FlowToken::Period;
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
		nextChar();
		return token_ = FlowToken::End;
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
		return parseString(false);
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
		return parseIdent();
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
	while (!eof() && std::isspace(currentChar()))
		nextChar();

	if (eof())
		return true;

	if (currentChar() == '#')
	{
		// skip chars until EOL
		for (;;)
		{
			if (eof())
			{
				token_ = FlowToken::Eof;
				return true;
			}

			if (currentChar() == '\n')
			{
				nextChar();
				return consumeSpace();
			}

			nextChar();
		}
	}

	if (currentChar() == '/')
	{
		if (peekChar() == '*') // "/*" ... "*/"
		{
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
		else if (peekChar() == '/') // "//" ... (EOL|EOF)
		{
			// skip chars until EOL
			for (;;)
			{
				if (eof())
				{
					token_ = FlowToken::Eof;
					return true;
				}

				if (currentChar() == '\n')
				{
					nextChar();
					return consumeSpace();
				}

				nextChar();
			}
		}
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

	if (!std::isalpha(currentChar()) && currentChar() != '_') {
		printf("parseIdent: unknown char %d (0x%02X)\n", currentChar(), currentChar());
		nextChar();
		return token_ = FlowToken::Unknown;
	}

	nextChar();

	while (std::isalnum(currentChar()) || currentChar() == '_' || currentChar() == '.')
	{
		stringValue_ += static_cast<char>(currentChar());
		nextChar();
	}

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
		{ "import", FlowToken::Import },
		{ "from", FlowToken::From },
		{ "handler", FlowToken::Handler },
		{ "extern", FlowToken::Extern },
		{ "and", FlowToken::And },
		{ "or", FlowToken::Or },
		{ "xor", FlowToken::Xor },
		{ "not", FlowToken::Not },
		{ "void", FlowToken::Void },
		{ "char", FlowToken::Char },
		{ "int", FlowToken::Int },
		{ "long", FlowToken::Long },
		{ "longlong", FlowToken::LongLong },
		{ "float", FlowToken::Float },
		{ "double", FlowToken::Double },
		{ "longdouble", FlowToken::LongDouble },
		{ "uchar", FlowToken::UChar },
		{ "ulong", FlowToken::ULong },
		{ "ulonglong", FlowToken::ULongLong },
		{ "string", FlowToken::String },
		{ 0, FlowToken::Unknown }
	};

	for (auto i = keywords; i->symbol; ++i)
		if (strcmp(i->symbol, stringValue_.c_str()) == 0)
			return token_ = i->token;

	if (stringValue_ == "true" || stringValue_ == "yes") {
		numberValue_ = 1;
		return token_ = FlowToken::Number;
	}

	if (stringValue_ == "false" || stringValue_ == "no") {
		numberValue_ = 0;
		return token_ = FlowToken::Number;
	}

	return token_ = FlowToken::Ident;
}

FlowToken FlowLexer::parseIPv6()
{
	return token_ = FlowToken::Unknown;
}

std::string FlowLexer::tokenString() const
{
	switch (token()) {
		case FlowToken::Ident:
			return std::string("ident:") + stringValue_;
		case FlowToken::RegExp:
			printf("tokenString returns: '%s'\n", stringValue_.c_str());
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

void FlowLexer::dump() const
{
	printf("[%04ld:%02ld] %3d %s\n",
			line(), column(),
			static_cast<int>(token()), tokenString().c_str());
}

} // namespace x0
