/* <x0/request.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/request.hpp>
#include <x0/response.hpp>
#include <strings.h>			// strcasecmp()

namespace x0 {

std::string request::header(const std::string& name) const
{
	for (std::vector<x0::header>::const_iterator i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		if (strcasecmp(i->name.c_str(), name.c_str()) == 0)
		{
			return i->value;
		}
	}

	return std::string();
}

} // namespace x0
