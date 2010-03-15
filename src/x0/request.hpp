/* <x0/request.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_hpp
#define x0_http_request_hpp (1)

#include <x0/io/fileinfo.hpp>
#include <x0/buffer.hpp>
#include <x0/header.hpp>
#include <x0/strutils.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>
#include <string>
#include <vector>
#include <boost/tuple/tuple.hpp>
#include <boost/logic/tribool.hpp>

namespace x0 {

class plugin;

//! \addtogroup core
//@{

/**
 * \brief a client HTTP reuqest object, holding the parsed x0 request data.
 *
 * \see header, response, connection, server
 */
struct X0_API request
{
public:
	explicit request(x0::connection& connection);

	x0::connection& connection;					///< the TCP/IP connection this request has been sent through

	// request properties
	buffer_ref method;							///< HTTP request method, e.g. HEAD, GET, POST, PUT, etc.
	buffer_ref uri;								///< parsed request uri
	buffer_ref path;							///< decoded path-part
	fileinfo_ptr fileinfo;						///< the final entity to be served, for example the full path to the file on disk.
	buffer_ref query;							///< decoded query-part
	int http_version_major;						///< HTTP protocol version major part that this request was formed in
	int http_version_minor;						///< HTTP protocol version minor part that this request was formed in
	std::vector<x0::request_header> headers;	///< request headers
	std::string body;							///< body

	/** retrieve value of a given request header */
	buffer_ref header(const std::string& name) const;

	// accumulated request data
	buffer_ref username;						///< username this client has authenticated with.
	std::string document_root;					///< the document root directory for this request.

//	std::string if_modified_since;		//!< "If-Modified-Since" request header value, if specified.
//	std::shared_ptr<range_def> range;	//!< parsed "Range" request header

	// custom data bindings
	std::map<plugin *, custom_data_ptr> custom_data;

	// utility methods
	bool supports_protocol(int major, int minor) const;
	std::string hostid() const;
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

//@}

} // namespace x0

#endif
