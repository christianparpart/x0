/* <x0/request_parser.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_parser_hpp
#define x0_http_request_parser_hpp (1)

#include <x0/message_parser.hpp>
#include <x0/buffer.hpp>
#include <x0/header.hpp>
#include <x0/request.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>

#include <string>
#include <functional>

#include <boost/logic/tribool.hpp>

#if 0
#	define TRACE(msg...)
#else
#	define TRACE(msg...) DEBUG("request_parser: " msg)
#endif

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
	enum state // {{{
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
		content
	}; // }}}

public:
	request_parser();

	void reset();
	std::size_t next_offset() const;
	boost::tribool parse(request& req, buffer_ref&& chunk);

private:
	request *request_;
	message_parser parser_;
	std::size_t next_offset_;
	bool completed_;

	static inline bool url_decode(buffer_ref& url);
	enum message_parser::state state() const;

	void on_request(buffer_ref&& method, buffer_ref&& path, buffer_ref&& protocol, int major, int minor);
	void on_header(buffer_ref&& name, buffer_ref&& value);
	void on_header_done();
	void on_content(buffer_ref&& chunk);
	bool on_complete();
};

// {{{ request_parser impl
inline request_parser::request_parser() :
	request_(0),
	parser_(message_parser::REQUEST),
	next_offset_(0),
	completed_(false)
{
	using namespace std::placeholders;
	parser_.on_request = std::bind(&request_parser::on_request, this, _1, _2, _3, _4, _5);
	parser_.on_header = std::bind(&request_parser::on_header, this, _1, _2);
	parser_.on_header_done = std::bind(&request_parser::on_header_done, this);
	parser_.on_content = std::bind(&request_parser::on_content, this, _1);
	parser_.on_complete = std::bind(&request_parser::on_complete, this);
}

inline void request_parser::reset()
{
	parser_.reset();
	next_offset_ = 0;
	completed_ = false;
}

inline std::size_t request_parser::next_offset() const
{
	return next_offset_;
}

/** parses partial HTTP request.
 *
 * \param r request to fill with parsed data
 * \param chunk holding the (possibly partial) chunk of the request to be parsed.
 *
 * \retval true request has been fully parsed.
 * \retval false HTTP request parser error (should result into bad_request if possible.)
 * \retval indeterminate parsial request parsed successfully but more input is needed to complete parsing.
 */
inline boost::tribool request_parser::parse(request& r, buffer_ref&& chunk)
{
	request_ = &r;
	completed_ = false;

	std::error_code ec;
	next_offset_ = parser_.parse(std::move(chunk), ec);

	if (completed_)
		// request fully parsed.
		return true;
	else if (!ec)
		// incomplete request chunk. need more input.
		return boost::indeterminate;
	else
		// syntax error
		return false;
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

inline enum message_parser::state request_parser::state() const
{
	return parser_.state();
}
// }}}

//@}

} // namespace x0

#undef TRACE

#endif
