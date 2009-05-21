/* <x0/request.hpp>
 * 
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_hpp
#define x0_http_request_hpp (1)

#include <string>
#include <vector>
#include <x0/header.hpp>
#include <x0/types.hpp>

namespace x0 {

/**
 * \ingroup core
 * \brief a client HTTP reuqest object, holding the parsed x0 request data.
 *
 * \see header, response, connection, server
 */
struct request
{
	/// the TCP/IP connection this request has been sent through
	x0::connection *connection;

	/// HTTP request method, e.g. GET, POST, etc.
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
	std::vector<header> headers;

	std::string get_header(const std::string& name) const;

	/// body
	std::string body;

	// -- accumulated request data

	/// username this client has authenticated with.
	std::string username;

	/// the document root directory for this request.
	std::string document_root;
};

} // namespace x0

#endif
