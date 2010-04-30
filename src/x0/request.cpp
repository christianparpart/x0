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

bool request::content_available() const
{
	return connection.state() != message_processor::MESSAGE_BEGIN;
}

bool request::read(const std::function<void(buffer_ref&&)>& callback)
{
	if (!content_available())
		return false;

	read_callback_ = callback;

	return true;
}

void request::on_read(buffer_ref&& chunk)
{
	if (read_callback_)
	{
		read_callback_(std::move(chunk));
		read_callback_ = std::function<void(buffer_ref&&)>();
	}
}

} // namespace x0
