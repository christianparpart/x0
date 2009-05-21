/* <x0/response.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_response_hpp
#define x0_response_hpp (1)

#include <x0/types.hpp>
#include <x0/header.hpp>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>

namespace x0 {

/**
 * \ingroup core
 * \brief HTTP response object.
 *
 * this response contains all information of data to be sent back to the requesting client.
 *
 * \see request, connection, server
 */
struct response
{
	// {{{ standard responses
	static response_ptr ok;
	static response_ptr created;
	static response_ptr accepted;
	static response_ptr no_content;
	static response_ptr multiple_choices;
	static response_ptr moved_permanently;
	static response_ptr moved_temporarily;
	static response_ptr not_modified;
	static response_ptr bad_request;
	static response_ptr unauthorized;
	static response_ptr forbidden;
	static response_ptr not_found;
	static response_ptr internal_server_error;
	static response_ptr not_implemented;
	static response_ptr bad_gateway;
	static response_ptr service_unavailable;
	// }}}

	/// HTTP response status code.
	int status;

	/// the headers to be included in the response.
	std::vector<header> headers;

	/// the content to be sent in the response.
	std::string content;

	/**
	 * convert the response into a vector of buffers. 
	 *
	 * The buffers do not own the underlying memory blocks,
	 * therefore the response object must remain valid and
	 * not be changed until the write operation has completed.
	 */
	std::vector<boost::asio::const_buffer> to_buffers();

public:
	/** creates an empty response object */
	response();

	/** constructs a standard response. */
	explicit response(int code);

	/** adds a response header. */
	response& operator+=(const header& hd);

	/** sets a response header value (overwrites existing one if already defined). */
	response& operator*=(const header& hd);

	/** checks wether given response header has been already defined. */
	bool has_header(const std::string& name) const;

	/** retrieves the value of a given header by name. */
	std::string get_header(const std::string& name) const;

public:
	static const char *status_cstr(int status);
	static std::string status_str(int status);

private:
	char status_buf[3];
};

} // namespace x0

#endif
