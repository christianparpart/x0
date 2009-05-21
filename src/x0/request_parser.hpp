/* <x0/request_parser.hpp>
 * 
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_parser_hpp
#define x0_http_request_parser_hpp (1)

#include <x0/types.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>

namespace x0 {

struct request;

class request_parser
{
public:
	request_parser();

	void reset();

	template<typename InputIterator>
	tuple<tribool, InputIterator> parse(request& req, InputIterator begin, InputIterator end)
	{
		while (begin != end)
		{
			tribool result = consume(req, *begin++);
			if (result || !result)
				return make_tuple(result, begin);
		}

		tribool result = indeterminate;
		return make_tuple(result, begin);
	}

	tribool consume(request& req, char input);

	static bool is_char(int ch);
	static bool is_ctl(int ch);
	static bool is_tspecial(int ch);
	static bool is_digit(int ch);

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
		expecting_newline_3
	} state_;
};

} // namespace x0

#endif
