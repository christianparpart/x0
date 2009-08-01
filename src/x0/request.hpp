/* <x0/request.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_hpp
#define x0_http_request_hpp (1)

#include <x0/header.hpp>
#include <x0/types.hpp>
#include <string>
#include <vector>
#include <boost/tuple/tuple.hpp>
#include <boost/logic/tribool.hpp>

namespace x0 {

/**
 * \ingroup core
 * \brief a client HTTP reuqest object, holding the parsed x0 request data.
 *
 * \see header, response, connection, server
 */
struct request
{
	class reader;

	explicit request(x0::connection& connection);

	/// the TCP/IP connection this request has been sent through
	x0::connection& connection;

	/// HTTP request method, e.g. HEAD, GET, POST, PUT, etc.
	std::string method;

	/// unparsed request uri
	std::string uri;

	/// decoded path-part
	std::string path;

	/// the final entity to be served, for example the full path to the file on disk.
	std::string entity;

	/// decoded query-part
	std::string query;

	int http_version_major;
	int http_version_minor;

	/// request headers
	std::vector<x0::header> headers;

	/** retrieve value of a given request header */
	std::string header(const std::string& name) const;

	/// body
	std::string body;

	// -- accumulated request data

	/// username this client has authenticated with.
	std::string username;

	/// the document root directory for this request.
	std::string document_root;

//	std::string if_modified_since;		//!< "If-Modified-Since" request header value, if specified.
//	std::shared_ptr<range_def> range;	//!< parsed "Range" request header

	bool supports_protocol(int major, int minor) const
	{
		if (major == http_version_major && minor <= http_version_minor)
			return true;

		if (major < http_version_major)
			return true;

		return false;
	}
};

/**
 * \ingroup core
 * \brief implements the HTTP request parser.
 *
 * \see request, connection
 */
class request::reader
{
public:
	reader();

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
		expecting_newline_3,
		reading_body
	} state_;
};

inline request::request(x0::connection& conn) :
	connection(conn)
{
}

} // namespace x0

#endif
