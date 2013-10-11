/* <x0/Error.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Error.h>
#include <x0/Defines.h>

namespace x0 {

class ErrorCategoryImpl :
	public std::error_category
{

	virtual const char *name() const noexcept(true)
	{
		return "x0";
	}

	virtual std::string message(int ec) const
	{
		static const char *msg[] = {
			"Success",
			"Config File Error",
			"Fork Error",
			"PID file not specified",
			"Cannot create PID file",
			"Could not initialize SSL library",
			"No HTTP Listeners defined"
		};
		return msg[ec];
	}

};

#ifndef __APPLE__
const std::error_category& errorCategory() throw()
{
	static ErrorCategoryImpl errorCategoryImpl_;
	return errorCategoryImpl_;
}
#endif

} // namespace x0

