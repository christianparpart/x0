/* <x0/request.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_hpp
#define x0_http_request_hpp (1)

#include <x0/buffer.hpp>
#include <x0/header.hpp>
#include <x0/fileinfo.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>
#include <string>
#include <vector>
#include <boost/tuple/tuple.hpp>
#include <boost/logic/tribool.hpp>

namespace x0 {

//! \addtogroup core
//@{

/**
 * \brief a client HTTP reuqest object, holding the parsed x0 request data.
 *
 * \see header, response, connection, server
 */
struct request
{
public:
	class reader;

public:
	explicit request(x0::connection& connection);

	/// the TCP/IP connection this request has been sent through
	x0::connection& connection;

public: // request properties
	/// HTTP request method, e.g. HEAD, GET, POST, PUT, etc.
	std::string method;

	/// unparsed request uri
	std::string uri;

	/// decoded path-part
	std::string path;

	/// the final entity to be served, for example the full path to the file on disk.
	fileinfo_ptr fileinfo;

	/// decoded query-part
	std::string query;

	/// HTTP protocol version major part that this request was formed in
	int http_version_major;
	/// HTTP protocol version minor part that this request was formed in
	int http_version_minor;

	/// request headers
	std::vector<x0::header> headers;

	/** retrieve value of a given request header */
	std::string header(const std::string& name) const;

	/// body
	std::string body;

public: // accumulated request data
	/// username this client has authenticated with.
	std::string username;

	/// the document root directory for this request.
	std::string document_root;

//	std::string if_modified_since;		//!< "If-Modified-Since" request header value, if specified.
//	std::shared_ptr<range_def> range;	//!< parsed "Range" request header

public: // utility methods
	bool supports_protocol(int major, int minor) const;
	std::string hostid() const;
};

/**
 * \brief implements the HTTP request parser.
 *
 * \see request, connection
 */
class request::reader
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

private:
	state state_;
	std::string buf_;

private:
	static inline bool is_char(int ch);
	static inline bool is_ctl(int ch);
	static inline bool is_tspecial(int ch);
	static inline bool is_digit(int ch);

	static inline bool url_decode(std::string& url);

public:
	reader();

	void reset();

	/** parses partial HTTP request.
	 *
	 * \param req request to fill with parsed data
	 * \param begin iterator to first byte-wise position of the HTTP request to parse
	 * \param end iterator marking the end of partial stream. this method will parse up to excluding this end of bytes.
	 *
	 * \retval true request has been fully parsed.
	 * \retval false HTTP request parser error (should result into bad_request if possible.)
	 * \retval indeterminate parsial request parsed successfully but more input is needed to complete parsing.
	 */
	inline boost::tribool parse(request& req, const buffer_ref& data);

private:
	/**
	 * \param r reference to the request to fill
	 * \param input the input character to process as part of the incoming request.
	 *
	 * \retval boost::indeterminate parsed successfully but request still incomplete.
	 * \retval true parsed successfully and request is now complete.
	 * \retval false parse error
	 */
	inline boost::tribool consume(request& r, char input);
};

// {{{ request impl
inline request::request(x0::connection& conn) :
	connection(conn)
{
}

inline bool request::supports_protocol(int major, int minor) const
{
	if (major == http_version_major && minor <= http_version_minor)
		return true;

	if (major < http_version_major)
		return true;

	return false;
}
// }}}

// {{{ request::reader impl
inline bool request::reader::is_char(int ch)
{
	return ch >= 0 && ch < 127;
}

inline bool request::reader::is_ctl(int ch)
{
	return (ch >= 0 && ch <= 31) || ch == 127;
}

inline bool request::reader::is_tspecial(int ch)
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

inline bool request::reader::is_digit(int ch)
{
	return ch >= '0' && ch <= '9';
}

inline bool request::reader::url_decode(std::string& url)
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

inline request::reader::reader() :
	state_(method_start), buf_()
{
}

inline void request::reader::reset()
{
	state_ = method_start;
	buf_.clear();
}

inline boost::tribool request::reader::parse(request& req, const buffer_ref& data)
{
	for (buffer_ref::const_iterator begin = data.begin(), end = data.end(); begin != end; ++begin)
	{
		boost::tribool result = consume(req, *begin);

		if (!indeterminate(result))
			return result;
	}

	return boost::indeterminate;
}

inline boost::tribool request::reader::consume(request& r, char input)
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
				buf_.push_back(input);
				return boost::indeterminate;
			}
		case method:
			if (input == ' ')
			{
				r.method = buf_;
				buf_.clear();
				state_ = uri;
				return boost::indeterminate;
			}
			else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
			{
				return false;
			}
			else
			{
				buf_.push_back(input);
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
				buf_.push_back(input);
				return boost::indeterminate;
			}
		case uri:
			if (input == ' ')
			{
				if (!url_decode(buf_))
					return false;

				r.uri = buf_;
				buf_.clear();

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
					return false;

				state_ = http_version_h;
				return boost::indeterminate;
			}
			else if (is_ctl(input))
			{
				return false;
			}
			else
			{
				buf_.push_back(input);
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
// }}}

//@}

} // namespace x0

#endif
