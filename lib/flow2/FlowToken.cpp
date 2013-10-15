#include <x0/flow2/FlowToken.h>
#include <cstdio>
#include <assert.h>

namespace x0 {

bool FlowTokenTraits::isKeyword(FlowToken t)
{
	switch (t) {
		case FlowToken::Var:
		case FlowToken::On:
		case FlowToken::Do:
		case FlowToken::If:
		case FlowToken::Then:
		case FlowToken::Else:
		case FlowToken::Unless:
		case FlowToken::Import:
		case FlowToken::From:
		case FlowToken::Handler:
			return true;
		default:
			return false;
	}
}

bool FlowTokenTraits::isReserved(FlowToken t)
{
	switch (t) {
		default:
			return false;
	}
}

bool FlowTokenTraits::isType(FlowToken t)
{
	switch (t) {
		case FlowToken::VoidType:
		case FlowToken::BoolType:
		case FlowToken::NumberType:
		case FlowToken::StringType:
			return true;
		default:
			printf("unknown token: %d\n", (int)t);
			return false;
	}
}

bool FlowTokenTraits::isOperator(FlowToken t)
{
	switch (t) {
		case FlowToken::Assign:
		case FlowToken::Question:
		case FlowToken::Equal:
		case FlowToken::UnEqual:
		case FlowToken::Less:
		case FlowToken::Greater:
		case FlowToken::LessOrEqual:
		case FlowToken::GreaterOrEqual:
		case FlowToken::PrefixMatch:
		case FlowToken::SuffixMatch:
		case FlowToken::RegexMatch:
		case FlowToken::HashRocket:
		case FlowToken::Plus:
		case FlowToken::Minus:
		case FlowToken::Mul:
		case FlowToken::Div:
		case FlowToken::Shl:
		case FlowToken::Shr:
		case FlowToken::Comma:
		case FlowToken::Pow:
			return true;
		default:
			return false;
	}
}

bool FlowTokenTraits::isUnaryOp(FlowToken t)
{
	// token Plus and Minus can be both, unary and binary
	switch (t) {
		case FlowToken::Minus:
		case FlowToken::Plus:
		case FlowToken::Not:
			return true;
		default:
			return false;
	}
}

bool FlowTokenTraits::isPrimaryOp(FlowToken t)
{
	return false;
}

bool FlowTokenTraits::isLiteral(FlowToken t)
{
	switch (t) {
		case FlowToken::InterpolatedStringFragment:
		case FlowToken::InterpolatedStringEnd:
		case FlowToken::Boolean:
		case FlowToken::Number:
		case FlowToken::String:
		case FlowToken::RawString:
		case FlowToken::RegExp:
		case FlowToken::IP:
			return true;
		default:
			return false;
	}
}

const char *FlowToken::c_str() const throw()
{
	switch (value_) {
		case FlowToken::Unknown: return "Unknown";
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
		case FlowToken::HashRocket: return "=>";
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
		case FlowToken::Unless: return "unless";
		case FlowToken::Import: return "import";
		case FlowToken::From: return "from";
		case FlowToken::Handler: return "handler";
		case FlowToken::VoidType: return "void()";
		case FlowToken::BoolType: return "bool()";
		case FlowToken::NumberType: return "int()";
		case FlowToken::StringType: return "string()";
		case FlowToken::Ident: return "Ident";
		case FlowToken::Period: return "Period";
		case FlowToken::DblPeriod: return "DblPeriod";
		case FlowToken::Ellipsis: return "Ellipsis";
		case FlowToken::Comment: return "Comment";
		case FlowToken::Eof: return "EOF";
		case FlowToken::InterpolatedStringFragment: return "InterpolatedStringFragment";
		case FlowToken::InterpolatedStringEnd: return "InterpolatedStringEnd";
		default: assert(!"FIXME: Invalid Token.");
	}
}

} // namespace x0
