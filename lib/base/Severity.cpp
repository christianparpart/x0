/* <x0/Severity.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/Severity.h>
#include <x0/strutils.h>
#include <map>

namespace x0 {

Severity::Severity(const std::string& value)
{
	std::map<std::string, Severity> map = {
		{ "err", Severity::error },
		{ "error", Severity::error },
		{ "", Severity::warn }, // empty string defaults to warning-level
		{ "warn", Severity::warn },
		{ "warning", Severity::warn },
		{ "notice", Severity::notice },
		{ "info", Severity::info },
		{ "debug", Severity::debug },
		{ "debug1", Severity::debug1 },
		{ "debug2", Severity::debug2 },
		{ "debug3", Severity::debug3 },
		{ "debug4", Severity::debug4 },
		{ "debug5", Severity::debug5 },
		{ "debug6", Severity::debug6 },
	};

	auto i = map.find(value);
	if (i != map.end()) {
		value_ = i->second;
	} else {
		value_ = std::max(std::min(9, atoi(value.c_str())), 0);
		// throw std::runtime_error(fstringbuilder::format("Invalid Severity '%s'", value.c_str()));
	}
}

const char *Severity::c_str() const
{
	switch (value_) {
		case error: return "error";
		case warning: return "warning";
		case notice: return "notice";
		case info: return "info";
		case debug1: return "debug:1";
		case debug2: return "debug:2";
		case debug3: return "debug:3";
		case debug4: return "debug:4";
		case debug5: return "debug:5";
		case debug6: return "debug:6";
		default: return "UNKNOWN";
	}
}

} // namespace x0
