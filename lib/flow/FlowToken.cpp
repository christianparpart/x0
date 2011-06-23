/* <flow/FlowToken.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://redmine.xzero.ws/projects/flow
 *
 * (c) 2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/flow/FlowToken.h>

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
		case FlowToken::Void:
		case FlowToken::Boolean:
		case FlowToken::Char:
		case FlowToken::Int:
		case FlowToken::Long:
		case FlowToken::LongLong:
		case FlowToken::Float:
		case FlowToken::Double:
		case FlowToken::LongDouble:
		case FlowToken::UChar:
		case FlowToken::UInt:
		case FlowToken::ULong:
		case FlowToken::ULongLong:
		case FlowToken::String:
			return true;
		default:
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
		case FlowToken::KeyAssign:
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
	switch (t)
	{
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

} // namespace x0
