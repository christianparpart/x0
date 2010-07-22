/* <x0/HttpRequest.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpRequest.h>
#include <x0/http/HttpResponse.h>
#include <x0/http/HttpConnection.h>
#include <x0/strutils.h>
#include <strings.h>			// strcasecmp()

namespace x0 {

BufferRef HttpRequest::header(const std::string& name) const
{
	for (std::vector<HttpRequestHeader>::const_iterator i = headers.begin(), e = headers.end(); i != e; ++i)
	{
		if (iequals(i->name, name))
		{
			return i->value;
		}
	}

	return BufferRef();
}

std::string HttpRequest::hostid() const
{
	if (hostid_.empty())
		hostid_ = x0::make_hostid(header("Host"), connection.local_port());

	return hostid_;
}

void HttpRequest::set_hostid(const std::string& value)
{
	hostid_ = value;
}

bool HttpRequest::content_available() const
{
	return connection.state() != HttpMessageProcessor::MESSAGE_BEGIN;
}

bool HttpRequest::read(const std::function<void(BufferRef&&)>& callback)
{
	if (!content_available())
		return false;

	read_callback_ = callback;

	return true;
}

void HttpRequest::on_read(BufferRef&& chunk)
{
	if (read_callback_)
	{
		read_callback_(std::move(chunk));
		read_callback_ = std::function<void(BufferRef&&)>();
	}
}

} // namespace x0
