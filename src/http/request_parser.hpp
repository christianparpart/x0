#ifndef x0_http_request_parser_hpp
#define x0_http_request_parser_hpp (1)

#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>

namespace http {

struct request;

class request_parser
{
public:
	request_parser();

	void reset();

	template<typename InputIterator>
	boost::tuple<boost::tribool, InputIterator> parse(request& req, InputIterator begin, InputIterator end)
	{
		while (begin != end)
		{
			boost::tribool result = consume(req, *begin++);
			if (result || !result)
				return boost::make_tuple(result, begin);
		}

		boost::tribool result = boost::indeterminate;
		return boost::make_tuple(result, begin);
	}

	boost::tribool consume(request& req, char input);

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

} // namespace http

#endif
