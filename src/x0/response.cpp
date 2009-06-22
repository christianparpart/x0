/* <x0/response.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/response.hpp>
#include <x0/debug.hpp>
#include <x0/types.hpp>

#include <boost/lexical_cast.hpp>
#include <string>

namespace x0 {

response::~response()
{
	DEBUG("response(%p, conn=%p)", this, connection_.get());
}

response& response::operator+=(const x0::header& value)
{
	headers.push_back(value);

	return *this;
}

response& response::operator*=(const x0::header& in)
{
	for (std::vector<x0::header>::iterator i = headers.begin(); i != headers.end(); ++i)
	{
		if (i->name == in.name)
		{
			i->value = in.value;
			return *this;
		}
	}

	headers.push_back(in);
	return *this;
}


bool response::has_header(const std::string& name) const
{
	for (std::vector<x0::header>::const_iterator i = headers.begin(); i != headers.end(); ++i)
	{
		if (i->name == name)
		{
			return true;
		}
	}

	return false;
}

std::string response::header(const std::string& name) const
{
	for (std::vector<x0::header>::const_iterator i = headers.begin(); i != headers.end(); ++i)
	{
		if (i->name == name)
		{
			return i->value;
		}
	}

	return std::string();
}

const std::string& response::header(const std::string& name, const std::string& value)
{
	for (std::vector<x0::header>::iterator i = headers.begin(); i != headers.end(); ++i)
	{
		if (i->name == name)
		{
			return i->value = value;
		}
	}

	headers.push_back(x0::header(name, value));
	return headers[headers.size() - 1].value;
}

composite_buffer response::serialize()
{
	composite_buffer buffers;

	if (!serializing_)
	{
		status_buf[0] = '0' + (status / 100);
		status_buf[1] = '0' + (status / 10 % 10);
		status_buf[2] = '0' + (status % 10);

		buffers.push_back("HTTP/1.1 ");
		buffers.push_back(status_buf);
		buffers.push_back(' ');
		buffers.push_back(status_cstr(status));
		buffers.push_back("\r\n");

		for (std::size_t i = 0; i < headers.size(); ++i)
		{
			const x0::header& h = headers[i];

			buffers.push_back(h.name.data(), h.name.size());
			buffers.push_back(": ");
			buffers.push_back(h.value.data(), h.value.size());
			buffers.push_back("\r\n");
		}

		buffers.push_back("\r\n");

		serializing_ = true;
	}

	buffers.push_back(content);

	return buffers;
}

response::response(connection_ptr conn, int _status) :
	connection_(conn),
	headers(),
	serializing_(false),
	status(_status)
{
	DEBUG("response(%p, conn=%p)", this, connection_.get());
}

const char *response::status_cstr(int value)
{
	switch (value)
	{
		case 200: return "Ok";
		case 201: return "Created";
		case 202: return "Accepted";
		case 204: return "No Content";
		case 300: return "Multiple Choices";
		case 301: return "Moved Permanently";
		case 302: return "Moved Temporarily";
		case 304: return "Not Modified";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 500: return "Internal Server Error";
		case 501: return "Not_Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		default: return "";
	}
}

std::string response::status_str(int value)
{
	return std::string(status_cstr(value));
}

} // namespace x0
