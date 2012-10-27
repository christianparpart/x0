/* <src/FlowContext.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#pragma once

#include <x0/RegExp.h>
#include <x0/Api.h>

namespace x0 {

class X0_API FlowContext
{
public:
	FlowContext();
	virtual ~FlowContext();

	RegExp::Result* regexMatch() {
		if (!regexMatch_)
			regexMatch_ = new RegExp::Result();

		return regexMatch_;
	}

private:
	RegExp::Result* regexMatch_;
};

} // namespace x0
