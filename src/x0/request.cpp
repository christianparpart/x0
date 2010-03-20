/* <x0/request.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/request.hpp>
#include <x0/response.hpp>
#include <x0/connection.hpp>
#include <x0/strutils.hpp>
#include <strings.h>			// strcasecmp()

namespace x0 {

buffer_ref request::header(const std::string& name) const
{
	for (std::vector<x0::request_header>::const_iterator i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		if (iequals(i->name, name))
		{
			return i->value;
		}
	}

	return buffer_ref();
}

std::string request::hostid() const
{
	if (hostid_.empty())
		hostid_ = x0::make_hostid(header("Host"), connection.local_port());

	return hostid_;
}

void request::set_hostid(const std::string& value)
{
	hostid_ = value;
}

} // namespace x0
