/* <x0/response.cpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/response.hpp>
#include <x0/types.hpp>

#include <boost/lexical_cast.hpp>
#include <string>

namespace x0 {

// {{{ standard responses
response_ptr response::ok(new response(200));
response_ptr response::created(new response(201));
response_ptr response::accepted(new response(202));
response_ptr response::no_content(new response(204));
response_ptr response::multiple_choices(new response(300));
response_ptr response::moved_permanently(new response(301));
response_ptr response::moved_temporarily(new response(302));
response_ptr response::not_modified(new response(304));
response_ptr response::bad_request(new response(400));
response_ptr response::unauthorized(new response(401));
response_ptr response::forbidden(new response(403));
response_ptr response::not_found(new response(404));
response_ptr response::internal_server_error(new response(500));
response_ptr response::not_implemented(new response(501));
response_ptr response::bad_gateway(new response(502));
response_ptr response::service_unavailable(new response(503));
// }}}

response& response::operator+=(const header& value)
{
	headers.push_back(value);

	return *this;
}

response& response::operator*=(const header& in)
{
	for (std::vector<header>::iterator i = headers.begin(); i != headers.end(); ++i)
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
	for (std::vector<header>::const_iterator i = headers.begin(); i != headers.end(); ++i)
	{
		if (i->name == name)
		{
			return true;
		}
	}

	return false;
}

std::string response::get_header(const std::string& name) const
{
	for (std::vector<header>::const_iterator i = headers.begin(); i != headers.end(); ++i)
	{
		if (i->name == name)
		{
			return i->value;
		}
	}

	return std::string();
}

std::vector<boost::asio::const_buffer> response::to_buffers()
{
	// {{{ static const's
	static const char http10_[] = { 'H', 'T', 'T', 'P', '/', '1', '.', '0', ' ' };
	static const char http11_[] = { 'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ' };
	static const char space[] = { ' ' };
	static const char name_value_separator[] = { ':', ' ' };
	static const char crlf[] = { '\r', '\n' };
	// }}}

	status_buf[0] = '0' + (status / 100);
	status_buf[1] = '0' + (status / 10 % 10);
	status_buf[2] = '0' + (status % 10);

	std::vector<boost::asio::const_buffer> buffers;

	buffers.push_back(boost::asio::buffer(http11_));
	buffers.push_back(boost::asio::buffer(status_buf));
	buffers.push_back(boost::asio::buffer(space));
	buffers.push_back(boost::asio::buffer(status_cstr(status)));
	buffers.push_back(boost::asio::buffer(crlf));

	for (std::size_t i = 0; i < headers.size(); ++i)
	{
		const header& h = headers[i];

		buffers.push_back(boost::asio::buffer(h.name));
		buffers.push_back(boost::asio::buffer(name_value_separator));
		buffers.push_back(boost::asio::buffer(h.value));
		buffers.push_back(boost::asio::buffer(crlf));
	}

	buffers.push_back(boost::asio::buffer(crlf));
	buffers.push_back(boost::asio::buffer(content));

	return buffers;
}

response::response() :
	status(0)
{
}

response::response(int status) :
	status(status)
{
	const char *codeStr = status_cstr(status);
	char buf[1024];

	snprintf(buf, sizeof(buf),
		"<html>"
		"<head><title>%s</title></head>"
		"<body><h1>%d %s</h1></body>"
		"</html>",
		codeStr, status, codeStr
	);
	content = buf;

	*this *= header("Content-Type", "text/html");
}

const char *response::status_cstr(int status)
{
	switch (status)
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

std::string response::status_str(int status)
{
	return std::string(status_cstr(status));
}

} // namespace x0
