/* <x0/NativeSymbol.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2011 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/NativeSymbol.h>
#include <x0/Buffer.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <dlfcn.h>

namespace x0 {

inline auto stripLeftOf(const char *value, char ch) -> const char *
{
	const char *p = value;

	for (auto i = value; *i; ++i)
		if (*i == ch)
			p = i;

	return p != value ? p + 1 : p;
}

Buffer& operator<<(Buffer& result, const NativeSymbol& s)
{
	Dl_info info;
	if (!dladdr(s.value(), &info))
		result.push_back("<unresolved symbol>");
	else if (!info.dli_sname || !*info.dli_sname)
		result.push_back("<invalid symbol>");
	else {
		char *rv = 0;
		int status = 0;
		std::size_t len = 2048;

		result.reserve(result.size() + len);

		try { rv = abi::__cxa_demangle(info.dli_sname, result.end(), &len, &status); }
		catch (...) {}

		if (status < 0)
			result.push_back(info.dli_sname);
		else
			result.resize(result.size() + strlen(rv));

		if (s.verbose()) {
			result.push_back(" in ");
			result.push_back(stripLeftOf(info.dli_fname, '/'));
		}
	}
	return result;
}

} // namespace x0
