#pragma once

#include <utility>
#include <memory>
#include <cstdint>
#include <x0/Api.h>

namespace x0 {

struct X0_API FlowToken
{
	enum _
	{
		Unknown,

		// literals
		Boolean, Number, String, RawString, RegExp, IP, Cidr,
        NamedParam,
		InterpolatedStringFragment, // "hello #{" or "} world #{"
		InterpolatedStringEnd,      // "} end"

		// symbols
		Assign, OrAssign, AndAssign,
		PlusAssign, MinusAssign, MulAssign, DivAssign,
		Semicolon, Question, Colon,
		And, Or, Xor,
		Equal, UnEqual, Less, Greater, LessOrEqual, GreaterOrEqual,
		PrefixMatch, SuffixMatch, RegexMatch, In, HashRocket,
		Plus, Minus, Mul, Div, Mod, Shl, Shr, Comma, Pow,
		Not, BitOr, BitAnd, BitXor,
		BrOpen, BrClose, RndOpen, RndClose, Begin, End,

		// keywords
		Var, Do, Handler,
		If, Then, Else, Unless, Match, On,
		Import, From,

		// data types
		VoidType, BoolType, NumberType, StringType,

		// misc
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

class X0_API FlowTokenTraits
{
public:
	static bool isKeyword(FlowToken t);
	static bool isReserved(FlowToken t);
	static bool isLiteral(FlowToken t);
	static bool isType(FlowToken t);

	static bool isOperator(FlowToken t);
	static bool isUnaryOp(FlowToken t);
	static bool isPrimaryOp(FlowToken t);
	static bool isRelOp(FlowToken t);
};

} // namespace x0

namespace std {
	template<> struct hash<x0::FlowToken> {
		uint32_t operator()(x0::FlowToken v) const {
			return static_cast<uint32_t>(v.value());
		}
	};
}
