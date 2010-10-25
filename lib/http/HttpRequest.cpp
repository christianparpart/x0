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
#include <x0/http/HttpListener.h>
#include <x0/strutils.h>
#include <strings.h>			// strcasecmp()

#define TRACE(msg...) DEBUG(msg)

namespace x0 {

void HttpRequest::updatePathInfo()
{
	if (!fileinfo)
		return;

	// split "/the/tail" from "/path/to/script.php/the/tail"

	std::string fullname(fileinfo->filename());
	struct stat st;
	size_t pos = std::string::npos;

	for (;;)
	{
		int rv = stat(fullname.c_str(), &st);
		if (rv == 0)
		{
			pathinfo = pos != std::string::npos ? fileinfo->filename().substr(pos) : "";
			fileinfo = connection.worker().fileinfo(fullname);
			return;
		}
		if (errno == ENOTDIR)
		{
			pos = fullname.rfind('/', pos - 1);
			fullname = fullname.substr(0, pos);
		}
		else
		{
			return;
		}
	}
}

BufferRef HttpRequest::requestHeader(const std::string& name) const
{
	for (std::vector<HttpRequestHeader>::const_iterator i = requestHeaders.begin(), e = requestHeaders.end(); i != e; ++i)
		if (iequals(i->name, name))
			return i->value;

	return BufferRef();
}

std::string HttpRequest::hostid() const
{
	if (hostid_.empty())
		hostid_ = x0::make_hostid(requestHeader("Host"), connection.listener().port());

	return hostid_;
}

void HttpRequest::setHostid(const std::string& value)
{
	hostid_ = value;
}

bool HttpRequest::contentAvailable() const
{
	return connection.state() != HttpMessageProcessor::MESSAGE_BEGIN;
}

/** setup request-body consumer callback.
 *
 * \param callback the callback to invoke on request-body chunks.
 *
 * \retval true callback set
 * \retval false callback not set (because there is no content available)
 */
bool HttpRequest::read(const std::function<void(BufferRef&&)>& callback)
{
	if (!contentAvailable())
		return false;

	if (expectingContinue)
	{
		TRACE("send 100-continue");

		connection.write<BufferSource>("HTTP/1.1 100 Continue\r\n\r\n");
		expectingContinue = false;
	}

	readCallback_ = callback;

	return true;
}

void HttpRequest::onRequestContent(BufferRef&& chunk)
{
	if (readCallback_)
	{
		readCallback_(std::move(chunk));
		readCallback_ = std::function<void(BufferRef&&)>();
	}
}

} // namespace x0
