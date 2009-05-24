/* <x0/request.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/request.hpp>

namespace x0 {

std::string request::get_header(const std::string& name) const
{
	for (std::vector<header>::const_iterator i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		if (i->name == name)
		{
			return i->value;
		}
	}

	return std::string();
}

} // namespace x0
