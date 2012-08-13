/* <flow/FlowToken.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.io/projects/flow
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_Flow_Token_h
#define sw_Flow_Token_h (1)

#include <map>

namespace x0 {

struct FlowToken
{
	enum _
	{
		Unknown,

		// literals (1..6)
		Boolean, Number, String, RawString, RegExp, IP,

		// symbols (7..34)
		Assign, OrAssign, AndAssign,
		PlusAssign, MinusAssign, MulAssign, DivAssign,
		Semicolon, Question, Colon,
		And, Or, Xor,
		Equal, UnEqual, Less, Greater, LessOrEqual, GreaterOrEqual,
		PrefixMatch, SuffixMatch, RegexMatch, In, KeyAssign,
		Plus, Minus, Mul, Div, Mod, Shl, Shr, Comma, Pow,
		Not, BitOr, BitAnd, BitXor,
		BrOpen, BrClose, RndOpen, RndClose, Begin, End,

		// keywords (35..42)
		Var, On, Do, Handler,
		If, Then, Else,
		Import, From,

		// data types (43..54)
		Void, Char, Int, Long, LongLong,
		Float, Double, LongDouble, UChar, UInt,
		ULong, ULongLong,

		// misc (55..60)
		Ident, Period, DblPeriod, Ellipsis, Comment, Eof,

		COUNT
	};

public:
	FlowToken() : value_(Unknown) {}
	FlowToken(int value) : value_(value) {}
	FlowToken(_ value) : value_(static_cast<int>(value)) {}

	int value() const throw() { return value_; }
	const char *c_str() const throw();

	operator int () const { return value_; }

private:
	int value_;
};

class FlowTokenTraits
{
public:
	static bool isKeyword(FlowToken t);
	static bool isReserved(FlowToken t);
	static bool isLiteral(FlowToken t);
	static bool isType(FlowToken t);

	static bool isOperator(FlowToken t);
	static bool isUnaryOp(FlowToken t);
	static bool isPrimaryOp(FlowToken t);
};

// {{{ inlines
inline const char *FlowToken::c_str() const throw()
{
	switch (value_)
	{
		case FlowToken::Boolean: return "Boolean";
		case FlowToken::Number: return "Number";
		case FlowToken::String: return "String";
		case FlowToken::RawString: return "RawString";
		case FlowToken::RegExp: return "RegExp";
		case FlowToken::IP: return "IP";
		case FlowToken::Assign: return "=";
		case FlowToken::OrAssign: return "|=";
		case FlowToken::AndAssign: return "&=";
		case FlowToken::PlusAssign: return "+=";
		case FlowToken::MinusAssign: return "-=";
		case FlowToken::MulAssign: return "*=";
		case FlowToken::DivAssign: return "/=";
		case FlowToken::Semicolon: return ";";
		case FlowToken::Question: return "?";
		case FlowToken::Colon: return ":";
		case FlowToken::And: return "and";
		case FlowToken::Or: return "or";
		case FlowToken::Xor: return "xor";
		case FlowToken::Equal: return "==";
		case FlowToken::UnEqual: return "!=";
		case FlowToken::Less: return "<";
		case FlowToken::Greater: return ">";
		case FlowToken::LessOrEqual: return "<=";
		case FlowToken::GreaterOrEqual: return ">=";
		case FlowToken::PrefixMatch: return "=^";
		case FlowToken::SuffixMatch: return "=$";
		case FlowToken::RegexMatch: return "=~";
		case FlowToken::KeyAssign: return "=>";
		case FlowToken::In: return "in";
		case FlowToken::Plus: return "+";
		case FlowToken::Minus: return "-";
		case FlowToken::Mul: return "*";
		case FlowToken::Div: return "/";
		case FlowToken::Mod: return "%";
		case FlowToken::Shl: return "shl";
		case FlowToken::Shr: return "shr";
		case FlowToken::Comma: return ",";
		case FlowToken::Pow: return "**";
		case FlowToken::Not: return "not";
		case FlowToken::BitOr: return "|";
		case FlowToken::BitAnd: return "&";
		case FlowToken::BitXor: return "^";
		case FlowToken::BrOpen: return "[";
		case FlowToken::BrClose: return "]";
		case FlowToken::RndOpen: return "(";
		case FlowToken::RndClose: return ")";
		case FlowToken::Begin: return "{";
		case FlowToken::End: return "}";
		case FlowToken::Var: return "var";
		case FlowToken::On: return "on";
		case FlowToken::Do: return "do";
		case FlowToken::If: return "if";
		case FlowToken::Then: return "then";
		case FlowToken::Else: return "else";
		case FlowToken::Import: return "import";
		case FlowToken::From: return "from";
		case FlowToken::Handler: return "handler";
		case FlowToken::Void: return "void";
		case FlowToken::Char: return "char";
		case FlowToken::Int: return "int";
		case FlowToken::Long: return "long";
		case FlowToken::LongLong: return "longlong";
		case FlowToken::Float: return "float";
		case FlowToken::Double: return "double";
		case FlowToken::LongDouble: return "longdouble";
		case FlowToken::UChar: return "uchar";
		case FlowToken::UInt: return "uint";
		case FlowToken::ULong: return "ulong";
		case FlowToken::ULongLong: return "ulonglong";
		case FlowToken::Ident: return "Ident";
		case FlowToken::Period: return "Period";
		case FlowToken::DblPeriod: return "DblPeriod";
		case FlowToken::Ellipsis: return "Ellipsis";
		case FlowToken::Comment: return "Comment";
		case FlowToken::Eof: return "EOF";
		default: return "UNKNOWN";
	}
}
// }}}

} // namespace Flow

#endif
