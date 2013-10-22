/* <x0/Severity.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#include <x0/Severity.h>
#include <x0/strutils.h>
#include <map>
#include <cstdlib>
#include <climits>

namespace x0 {

Severity::Severity(const std::string& value)
{
	set(value.c_str());
}

const char *Severity::c_str() const
{
	switch (value_) {
		case emerg: return "emerg";
		case alert: return "alert";
		case crit: return "crit";
		case error: return "error";
		case warning: return "warning";
		case notice: return "notice";
		case info: return "info";
		case debug1: return "debug:1";
		case debug2: return "debug:2";
		case debug3: return "debug:3";
		default: return "UNKNOWN";
	}
}

bool Severity::set(const char* value)
{
	std::map<std::string, Severity> map = {
		{ "emerg", Severity::emerg },
		{ "alert", Severity::alert },
		{ "crit", Severity::crit },
		{ "error", Severity::error },
		{ "warn", Severity::warn },
		{ "warning", Severity::warn },
		{ "notice", Severity::notice },
		{ "info", Severity::info },
		{ "debug", Severity::debug },
		{ "debug1", Severity::debug1 },
		{ "debug2", Severity::debug2 },
		{ "debug3", Severity::debug3 },
	};

	auto i = map.find(value);
	if (i != map.end()) {
		value_ = i->second;
		return true;
	} else {
		char* eptr = nullptr;
		errno = 0;

		long int result = strtol(value, &eptr, 10);

		if ((errno == ERANGE && (result == LONG_MAX || result == LONG_MIN)) || (errno != 0 && result == 0)) {
			perror("strtol");
			return false;
		}

		if (eptr == value) {
			// no digits found
			return false;
		}

		if (*eptr != '\0') {
			// trailing garbage
			return false;
		}

		if (result < 0 || result > 9) { // severity ranges are 0..9
			return false;
		}

		value_ = result;
		return true;
	}
}

} // namespace x0
