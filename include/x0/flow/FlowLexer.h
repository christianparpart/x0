/* <flow/FlowLexer.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_flow_Lexer_h
#define sw_flow_Lexer_h (1)

#include <x0/flow/FlowToken.h>
#include <x0/IPAddress.h>
#include <x0/Api.h>

#include <map>
#include <string>
#include <cstdio>
#include <istream>

namespace x0 {

struct X0_API FilePos // {{{
{
public:
	FilePos() : line(0), column(0), offset(0) {}
	FilePos(size_t r, size_t c, size_t o) : line(r), column(c), offset(o) {}

	FilePos& set(size_t r, size_t c, size_t o)
	{
		line = r;
		column = c;
		offset = o;

		return *this;
	}

	size_t line;
	size_t column;
	size_t offset;
}; // }}}

struct X0_API SourceLocation // {{{
{
public:
	SourceLocation() : fileName(), begin(), end() {}
	SourceLocation(const std::string& _fileName) : fileName(_fileName), begin(), end() {}
	SourceLocation(const std::string& _fileName, FilePos _beg, FilePos _end) : fileName(_fileName), begin(_beg), end(_end) {}

	std::string fileName;
	FilePos begin;
	FilePos end;

	SourceLocation& update(const FilePos& endPos) { end = endPos; return *this; }

	std::string dump(const std::string& prefix = std::string()) const;
	std::string text() const;
}; // }}}

class X0_API FlowLexer
{
public:
	enum Error {
		StringExceedsLine, IllegalChar, UnexpectedEof, 
		ErrorInInteger, NoOpenFile, InvalidStream,
		IllegalNumberFormat
	};

private:
	std::string filename_;
	std::istream *stream_;

	FilePos lastPos_;
	FilePos currPos_;
	FilePos nextPos_;
	SourceLocation currLocation_;
	SourceLocation lastLocation_;
	std::string content_;

	char currentChar_;
	size_t ipv6HexDigits_;

	long long numberValue_;
	IPAddress ipValue_;
	std::string stringValue_;
	FlowToken token_;

	int interpolationDepth_;

public:
	FlowLexer();
	~FlowLexer();

	const FilePos& lastPos() const { return lastPos_; }

	bool initialize(std::istream *input, const std::string& name = std::string());

	size_t line() const;
	size_t column() const;
	bool eof() const;

	std::string stringValue() const;
	long long numberValue() const;
	bool booleanValue() const;
	const IPAddress& ipValue() const;

	// char-wise operations
	int currentChar() const;
	int peekChar();
	int nextChar();
	bool advanceUntil(char value);

	// token-wise operations
	FlowToken token() const;
	FlowToken nextToken();
	std::string tokenString() const;
	std::string tokenToString(FlowToken value) const;

	const SourceLocation& location() const;
	const SourceLocation& lastLocation() const;
	std::string locationContent();

	std::string dump() const;

private:
	bool isHexChar() const;
	bool consume(char c);
	bool consumeSpace();
	FlowToken parseNumber();
	FlowToken parseString(bool raw);
	FlowToken parseString(char delimiter, FlowToken result);
	FlowToken parseInterpolationFragment(bool start);
	FlowToken parseIdent();

	FlowToken parseIPv6();
	FlowToken continueParseIPv6(bool firstComplete);
	bool ipv6HexPart();
	bool ipv6HexSeq();
	bool ipv6HexDigit4();
};

} // namespace x0

#endif
