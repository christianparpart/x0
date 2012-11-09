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

		// keywords (35..43)
		Var, On, Do, Handler,
		If, Then, Else, Unless,
		Import, From,

		// data types (44..46)
		VoidType, BoolType, IntType, StringType,

		// misc (47..52)
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

} // namespace Flow

#endif
