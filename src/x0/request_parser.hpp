/* <x0/request_parser.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_parser_hpp
#define x0_http_request_parser_hpp (1)

#include <x0/io/buffer.hpp>
#include <x0/header.hpp>
#include <x0/request.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>
#include <string>
#include <boost/tuple/tuple.hpp>
#include <boost/logic/tribool.hpp>

namespace x0 {

//! \addtogroup core
//@{

/**
 * \brief implements the HTTP request parser.
 *
 * \see request, connection, response_parser
 */
class request_parser
{
public:
	enum state
	{
		method_start,
		method,
		uri_start,
		uri,
		http_version_h,
		http_version_t_1,
		http_version_t_2,
		http_version_p,
		http_version_slash,
		http_version_major_start,
		http_version_major,
		http_version_minor_start,
		http_version_minor,
		expecting_newline_1,
		header_line_start,
		header_lws,
		header_name,
		space_before_header_value,
		header_value,
		expecting_newline_2,
		expecting_newline_3,
		reading_body
	};

public:
	request_parser();

	void reset();

	boost::tribool parse(request& req, const buffer_ref& data);

private:
	state state_;
	std::size_t left_;

private:
	static inline bool is_char(int ch);
	static inline bool is_ctl(int ch);
	static inline bool is_tspecial(int ch);
	static inline bool is_digit(int ch);

	static inline bool url_decode(buffer_ref& url);
};

// {{{ request_parser impl
inline bool request_parser::is_char(int ch)
{
	return ch >= 0 && ch < 127;
}

inline bool request_parser::is_ctl(int ch)
{
	return (ch >= 0 && ch <= 31) || ch == 127;
}

inline bool request_parser::is_tspecial(int ch)
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

inline bool request_parser::is_digit(int ch)
{
	return ch >= '0' && ch <= '9';
}

inline bool request_parser::url_decode(buffer_ref& url)
{
	std::size_t left = url.offset();
	std::size_t right = left + url.size();
	std::size_t i = left; // read pos
	std::size_t d = left; // write pos
	buffer& value = url.buffer();

	while (i != right)
	{
		if (value[i] == '%')
		{
			if (i + 3 <= right)
			{
				int ival;
				if (hex2int(value.begin() + i + 1, value.begin() + i + 3, ival))
				{
					value[d++] = static_cast<char>(ival);
					i += 3;
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
		else if (value[i] == '+')
		{
			value[d++] = ' ';
			++i;
		}
		else if (d != i)
		{
			value[d++] = value[i++];
		}
		else
		{
			++d;
			++i;
		}
	}

	url = value.ref(left, d - left);
	return true;
}

inline request_parser::request_parser() :
	state_(method_start), left_(0)
{
}

inline void request_parser::reset()
{
	state_ = method_start;
	left_ = 0;
}

/** parses partial HTTP request.
 *
 * \param r request to fill with parsed data
 * \param data buffer holding the (possibly partial) data of the request to be parsed.
 *
 * \retval true request has been fully parsed.
 * \retval false HTTP request parser error (should result into bad_request if possible.)
 * \retval indeterminate parsial request parsed successfully but more input is needed to complete parsing.
 */
inline boost::tribool request_parser::parse(request& r, const buffer_ref& data)
{
	std::size_t cur = data.offset();
	std::size_t count = cur + data.size();
	buffer_ref::const_iterator i = data.begin();

	for (; cur != count; ++cur)
	{
		char input = *i++;

		switch (state_)
		{
			case method_start:
				if (!is_char(input) || is_ctl(input) || is_tspecial(input))
					return false;

				state_ = method;
				break;
			case method:
				if (input == ' ')
				{
					r.method = data.buffer().ref(left_, cur - left_);
					state_ = uri;
					left_ = cur + 1;
				}
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
					return false;

				break;
			case uri_start:
				if (is_ctl(input))
					return false;

				state_ = uri;
				break;
			case uri:
				if (input == ' ')
				{
					r.uri = data.buffer().ref(left_, cur - left_);
					left_ = cur + 1;

					if (!url_decode(r.uri))
						return false;

					std::size_t n = r.uri.find("?");
					if (n != std::string::npos)
					{
						r.path = r.uri.ref(0, n);
						r.query = r.uri.ref(n + 1);
					}
					else
					{
						r.path = r.uri;
					}

					if (r.path.empty() || r.path[0] != '/' || r.path.find("..") != std::string::npos)
						return false;

					state_ = http_version_h;
				}
				else if (is_ctl(input))
					return false;

				break;
			case http_version_h:
				if (input != 'H')
					return false;

				state_ = http_version_t_1;
				break;
			case http_version_t_1:
				if (input != 'T')
					return false;

				state_ = http_version_t_2;
				break;
			case http_version_t_2:
				if (input != 'T')
					return false;

				state_ = http_version_p;
				break;
			case http_version_p:
				if (input != 'P')
					return false;

				state_ = http_version_slash;
				break;
			case http_version_slash:
				if (input != '/')
					return false;

				r.http_version_major = 0;
				r.http_version_minor = 0;

				state_ = http_version_major_start;
				break;
			case http_version_major_start:
				if (!is_digit(input))
					return false;

				r.http_version_major = r.http_version_major * 10 + input - '0';
				state_ = http_version_major;
				break;
			case http_version_major:
				if (input == '.')
					state_ = http_version_minor_start;
				else if (is_digit(input))
					r.http_version_major = r.http_version_major * 10 + input - '0';
				else
					return false;

				break;
			case http_version_minor_start:
				if (input == '\r')
					state_ = expecting_newline_1;
				else if (is_digit(input))
					r.http_version_minor = r.http_version_minor * 10 + input - '0';
				else
					return false;

				break;
			case expecting_newline_1:
				if (input != '\n')
					return false;

				state_ = header_line_start;
				break;
			case header_line_start:
				if (input == '\r')
					state_ = expecting_newline_3;
				else if (!r.headers.empty() && (input == ' ' || input == '\t'))
					state_ = header_lws; // header-value continuation
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
					return false;
				else
				{
					r.headers.push_back(x0::request_header());
					//r.headers.back().name.push_back(input);
					r.headers.back().name = data.buffer().ref(cur, 1);

					state_ = header_name;
				}
				break;
			case header_lws:
				if (input == '\r')
					state_ = expecting_newline_2;
				else if (input != ' ' && input != '\t')
				{
					state_ = header_value;
					r.headers.back().value = data.buffer().ref(cur, 1);
				}
				break;
			case header_name:
				if (input == ':')
					state_ = space_before_header_value;
				else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
					return false;
				else
					r.headers.back().name.shr(1);

				break;
			case space_before_header_value:
				if (input != ' ')
					return false;

				state_ = header_value;
				break;
			case header_value:
				if (input == '\r')
					state_ = expecting_newline_2;
				else if (is_ctl(input))
					return false;
				else if (r.headers.back().value.empty())
					r.headers.back().value = data.buffer().ref(cur, 1);
				else
					r.headers.back().value.shr(1);

				break;
			case expecting_newline_2:
				if (input != '\n')
					return false;

				state_ = header_line_start;
				break;
			case expecting_newline_3:
				if (input == '\n')
				{
					buffer_ref s(r.header("Content-Length"));
					if (!s.empty())
					{
						state_ = reading_body;
						r.body.reserve(std::atoi(s.data()));
						break;
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
					break;
				}
				else
				{
					return true;
				}
			default:
				return false;
		}
	}

	// request header parsed partially
	return boost::indeterminate;
}
// }}}

//@}

} // namespace x0

#endif
