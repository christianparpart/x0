/* <HttpRequest.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef x0_http_request_h
#define x0_http_request_h (1)

#include <x0/http/HttpHeader.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpServer.h>
#include <x0/io/FileInfo.h>
#include <x0/Severity.h>
#include <x0/Buffer.h>
#include <x0/BufferRef.h>
#include <x0/strutils.h>
#include <x0/Types.h>
#include <x0/Api.h>

#include <string>
#include <vector>

namespace x0 {

class HttpPlugin;
class HttpConnection;

//! \addtogroup core
//@{

/**
 * \brief a client HTTP reuqest object, holding the parsed x0 request data.
 *
 * \see header, response, HttpConnection, server
 */
struct X0_API HttpRequest
{
public:
	explicit HttpRequest(HttpConnection& connection);
	~HttpRequest();

	HttpConnection& connection;					///< the TCP/IP connection this request has been sent through

	// request properties
	BufferRef method;							///< HTTP request method, e.g. HEAD, GET, POST, PUT, etc.
	BufferRef uri;								///< parsed request uri
	BufferRef path;								///< decoded path-part
	FileInfoPtr fileinfo;						///< the final entity to be served, for example the full path to the file on disk.
	std::string pathinfo;
	BufferRef query;							///< decoded query-part
	int httpVersionMajor;						///< HTTP protocol version major part that this request was formed in
	int httpVersionMinor;						///< HTTP protocol version minor part that this request was formed in
	BufferRef hostname;							///< Host header field.
	std::vector<HttpRequestHeader> requestHeaders; ///< request headers

	/** retrieve value of a given request header */
	BufferRef requestHeader(const std::string& name) const;

	void updatePathInfo();

	// accumulated request data
	BufferRef username;							///< username this client has authenticated with.
	std::string documentRoot;					///< the document root directory for this request.

//	std::string if_modified_since;				//!< "If-Modified-Since" request header value, if specified.
//	std::shared_ptr<range_def> range;			//!< parsed "Range" request header
	bool expectingContinue;

	// custom data bindings
	std::map<HttpPlugin *, CustomDataPtr> customData;

	// utility methods
	bool supportsProtocol(int major, int minor) const;
	std::string hostid() const;
	void setHostid(const std::string& custom);

	// content management
	bool contentAvailable() const;
	bool read(const std::function<void(BufferRef&&)>& callback);

	template<typename... Args>
	void log(Severity s, Args&&... args);

private:
	mutable std::string hostid_;
	std::function<void(BufferRef&&)> readCallback_;

	void onRequestContent(BufferRef&& chunk);

	friend class HttpConnection;
};

// {{{ request impl
inline HttpRequest::HttpRequest(HttpConnection& conn) :
	connection(conn),
	method(),
	uri(),
	path(),
	fileinfo(),
	pathinfo(),
	query(),
	httpVersionMajor(0),
	httpVersionMinor(0),
	requestHeaders(),
	username(),
	documentRoot(),
	expectingContinue(false),
	customData(),
	hostid_(),
	readCallback_()
{
}

inline HttpRequest::~HttpRequest()
{
}

inline bool HttpRequest::supportsProtocol(int major, int minor) const
{
	if (major == httpVersionMajor && minor <= httpVersionMinor)
		return true;

	if (major < httpVersionMajor)
		return true;

	return false;
}

template<typename... Args>
inline void HttpRequest::log(Severity s, Args&&... args)
{
	connection.worker().server().log(s, args...);
}
// }}}

//@}

} // namespace x0

#endif
