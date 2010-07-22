/* <x0/Severity.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Severity.h>
#include <x0/strutils.h>

namespace x0 {

Severity::Severity(const std::string& name)
{
	if (name == "error")
		value_ = error;
	else if (name == "warn" || name == "warning" || name.empty()) // <- default: warn
		value_ = warn;
	else if (name == "info")
		value_ = info;
	else if (name == "debug")
		value_ = debug;
	else
		throw std::runtime_error(fstringbuilder::format("Invalid Severity '%s'", name.c_str()));
}

const char *Severity::c_str() const
{
	switch (value_)
	{
		case error: return "error";
		case warn: return "warn";
		case info: return "info";
		default: return "debug";
	}
}

} // namespace x0
