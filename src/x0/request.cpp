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

request::reader::reader() :
	state_(method_start)
{
}

void request::reader::reset()
{
	state_ = method_start;
}

static inline bool url_decode(std::string& url)
{
	std::string out;
	out.reserve(url.size());

	for (std::size_t i = 0; i < url.size(); ++i)
	{
		if (url[i] == '%')
		{
			if (i + 3 <= url.size())
			{
				int value;
				std::istringstream is(url.substr(i + 1, 2));
				if (is >> std::hex >> value)
				{
					out += static_cast<char>(value);
					i += 2;
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else if (url[i] == '+')
		{
			out += ' ';
		}
		else
		{
			out += url[i];
		}
	}

	url = out;
	return true;
}

/**
 * \param r reference to the request to fill
 * \param input the input character to process as part of the incoming request.
 *
 * \retval boost::indeterminate parsed successfully but request still incomplete.
 * \retval true parsed successfully and request is now complete.
 * \retval false parse error
 */
boost::tribool request::reader::consume(request& r, char input)
{
	switch (state_)
	{
		case method_start:
			if (!is_char(input) || is_ctl(input) || is_tspecial(input))
			{
				return false;
			}
			else
			{
				state_ = method;
				r.method.push_back(input);
				return boost::indeterminate;
			}
		case method:
			if (input == ' ')
			{
				state_ = uri;
				return boost::indeterminate;
			}
			else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
			{
				return false;
			}
			else
			{
				r.method.push_back(input);
				return boost::indeterminate;
			}
		case uri_start:
			if (is_ctl(input))
			{
				return false;
			}
			else
			{
				state_ = uri;
				r.uri.push_back(input);
				return boost::indeterminate;
			}
		case uri:
			if (input == ' ')
			{
				if (!url_decode(r.uri))
				{
					throw response::bad_request;
				}

				std::size_t n = r.uri.find("?");
				if (n != std::string::npos)
				{
					r.path = r.uri.substr(0, n);
					r.query = r.uri.substr(n + 1);
				}
				else
				{
					r.path = r.uri;
				}

				if (r.path.empty() || r.path[0] != '/' || r.path.find("..") != std::string::npos)
				{
					throw response::bad_request;
				}

				state_ = http_version_h;
				return boost::indeterminate;
			}
			else if (is_ctl(input))
			{
				return false;
			}
			else
			{
				r.uri.push_back(input);
				return boost::indeterminate;
			}
		case http_version_h:
			if (input == 'H')
			{
				state_ = http_version_t_1;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case http_version_t_1:
			if (input == 'T')
			{
				state_ = http_version_t_2;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case http_version_t_2:
			if (input == 'T')
			{
				state_ = http_version_p;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case http_version_p:
			if (input == 'P')
			{
				state_ = http_version_slash;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case http_version_slash:
			if (input == '/')
			{
				r.http_version_major = 0;
				r.http_version_minor = 0;

				state_ = http_version_major_start;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case http_version_major_start:
			if (is_digit(input))
			{
				r.http_version_major = r.http_version_major * 10 + input - '0';
				state_ = http_version_major;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case http_version_major:
			if (input == '.')
			{
				state_ = http_version_minor_start;
				return boost::indeterminate;
			}
			else if (is_digit(input))
			{
				r.http_version_major = r.http_version_major * 10 + input - '0';
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case http_version_minor_start:
			if (input == '\r')
			{
				state_ = expecting_newline_1;
				return boost::indeterminate;
			}
			else if (is_digit(input))
			{
				r.http_version_minor = r.http_version_minor * 10 + input - '0';
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case expecting_newline_1:
			if (input == '\n')
			{
				state_ = header_line_start;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case header_line_start:
			if (input == '\r')
			{
				state_ = expecting_newline_3;
				return boost::indeterminate;
			}
			else if (!r.headers.empty() && (input == ' ' || input == '\t'))
			{
				state_ = header_lws;
				return boost::indeterminate;
			}
			else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
			{
				return false;
			}
			else
			{
				r.headers.push_back(x0::header());
				r.headers.back().name.push_back(input);
				state_ = header_name;
				return boost::indeterminate;
			}
		case header_lws:
			if (input == '\r')
			{
				state_ = expecting_newline_2;
				return boost::indeterminate;
			}
			else if (input == ' ' || input == '\t')
			{
				return boost::indeterminate;
			}
			else
			{
				state_ = header_value;
				r.headers.back().value.push_back(input);
				return boost::indeterminate;
			}
		case header_name:
			if (input == ':')
			{
				state_ = space_before_header_value;
				return boost::indeterminate;
			}
			else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
			{
				return false;
			}
			else
			{
				r.headers.back().name.push_back(input);
				return boost::indeterminate;
			}
		case space_before_header_value:
			if (input == ' ')
			{
				state_ = header_value;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case header_value:
			if (input == '\r')
			{
				state_ = expecting_newline_2;
				return boost::indeterminate;
			}
			else if (is_ctl(input))
			{
				return false;
			}
			else
			{
				r.headers.back().value.push_back(input);
				return boost::indeterminate;
			}
		case expecting_newline_2:
			if (input == '\n')
			{
				state_ = header_line_start;
				return boost::indeterminate;
			}
			else
			{
				return false;
			}
		case expecting_newline_3:
			if (input == '\n')
			{
				std::string s(r.header("Content-Length"));
				if (!s.empty())
				{
					state_ = reading_body;
					r.body.reserve(std::atoi(s.c_str()));
					return boost::indeterminate;
				}
				else
				{
					return true;
				}
			}
			else
			{
				return false;
			}
		case reading_body:
			r.body.push_back(input);

			if (r.body.length() < r.body.capacity())
			{
				return boost::indeterminate;
			}
			else
			{
				return true;
			}
		default:
			return false;
	}
}

bool request::reader::is_char(int ch)
{
	return ch >= 0 && ch < 127;
}

bool request::reader::is_ctl(int ch)
{
	return (ch >= 0 && ch <= 31) || ch == 127;
}

bool request::reader::is_tspecial(int ch)
{
	switch (ch)
	{
		case '(':
		case ')':
		case '<':
		case '>':
		case '@':
		case ',':
		case ';':
		case ':':
		case '\\':
		case '"':
		case '/':
		case '[':
		case ']':
		case '?':
		case '=':
		case '{':
		case '}':
		case ' ':
		case '\t':
			return true;
		default:
			return false;
	}
}

bool request::reader::is_digit(int ch)
{
	return ch >= '0' && ch <= '9';
}

} // namespace x0
