/* <x0/response.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_response_hpp
#define x0_response_hpp (1)

#include <x0/types.hpp>
#include <x0/property.hpp>
#include <x0/header.hpp>
#include <x0/connection.hpp>
#include <x0/io/source.hpp>
#include <x0/io/filter_source.hpp>
#include <x0/io/chain_filter.hpp>
#include <x0/api.hpp>

#include <asio.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>

namespace x0 {

//! \addtogroup core
//@{

class request;

/**
 * \brief HTTP response object.
 *
 * This response contains all information of data to be sent back to the requesting client.
 *
 * A response contains of three main data sections:
 * <ul>
 *   <li>response status</li>
 *   <li>response headers</li>
 *   <li>response body</li>
 * </ul>
 *
 * The response status determins wether the request could be fully handled or 
 * for whatever reason not.
 *
 * The response headers is a list of key/value pairs of standard HTTP/1.1 including application
 * dependant fields.
 *
 * The response body contains the actual content, and must be exactly as large as specified in
 * the "Content-Length" response header.
 *
 * However, if no "Content-Length" response header is specified, it is assured that 
 * this response will be the last response being transmitted through this very connection,
 * though, having keep-alive disabled.
 * The response status line and headers transmission will be started automatically as soon as the
 * first write occurs.
 * If this response meant to contain no body, then the transmit operation may be started explicitely.
 *
 * \note All response headers and status information <b>must</b> be fully defined before the first content write operation.
 * \see response::flush(), request, connection, server
 */
class response
{
public:
	// {{{ standard response types
	enum code_type {
		continue_ = 100,
		switching_protocols = 101,

		ok = 200,
		created = 201,
		accepted = 202,
		non_authoriative_information = 203,
		no_content = 204,
		reset_content = 205,
		partial_content = 206,

		multiple_choices = 300,
		moved_permanently = 301,
		moved_temporarily = 302,
		not_modified = 304,

		bad_request = 400,
		unauthorized = 401,
		forbidden = 403,
		not_found = 404,
		requested_range_not_satisfiable = 416,

		internal_server_error = 500,
		not_implemented = 501,
		bad_gateway = 502,
		service_unavailable = 503
	};
	// }}}

private:
	/// pre-computed string representations of status codes, ready to be used by serializer
	static char status_codes[512][4];

	/// reference to the connection this response belongs to.
	connection_ptr connection_;

	/// reference to the related request.
	x0::request *request_;

	// state whether response headers have been already sent or not.
	bool headers_sent_;

public:
	/** Creates an empty response object.
	 *
	 * \param connection the connection this response is going to be transmitted through
	 * \param request the corresponding request object. <b>We take over ownership of it!</b>
	 * \param status initial response status code.
	 *
	 * \note this response object takes over ownership of the request object.
	 */
	response(connection_ptr connection, x0::request *request, int _status = 0);
	~response();

	/** retrieves a reference to the corresponding request object. */
	x0::request& request() const;

	/// HTTP response status code.
	value_property<int> status;

	// {{{ header manipulation
	/// the headers to be included in the response.
	std::vector<x0::response_header> headers;

	/** adds a response header. */
	response& operator+=(const x0::response_header& hd);

	/** sets a response header value (overwrites existing one if already defined). */
	response& operator*=(const x0::response_header& hd);

	/** checks wether given response header has been already defined. */
	bool has_header(const std::string& name) const;

	/** retrieves the value of a given header by name. */
	std::string header(const std::string& name) const;

	/** sets a response header */
	const std::string& header(const std::string& name, const std::string& value);
	// }}}

	/** returns true in case serializing the response has already been started, that is, headers has been sent out already. */
	bool headers_sent() const;

	/** write given source to response content and invoke the completion handler when done.
	 *
	 * \note this implicitely flushes the response-headers if not yet done, thus, making it impossible to modify them after this write.
	 *
	 * \param source the content to push to the client
	 * \param handler completion handler to invoke when source has been fully flushed or if an error occured
	 */
	void write(const source_ptr& source, const completion_handler_type& handler);

private:
	/** finishes this response by flushing the content into the stream.
	 *
	 * \note this also queues the underlying connection for processing the next request.
	 */
	void finish()
	{
		if (!headers_sent_) // nothing sent to client yet -> sent default status page
		{
			if (!status) // no status set -> default to 404 (not found)
				status = response::not_found;

			write(make_default_content(), boost::bind(&response::finished, this, asio::placeholders::error));
		}
		else
		{
			finished(asio::error_code());
		}
	}

	void complete_write(const asio::error_code& ec, const source_ptr& content, const completion_handler_type& handler);
	void write_content(const source_ptr& content, const completion_handler_type& handler);

	/** to be called <b>once</b> in order to initialize this class for instanciation.
	 *
	 * \note this is done automatically by server constructor.
	 * \see server
	 */
	static void initialize();
	friend class server;
	friend class connection;

public:
	static const char *status_cstr(int status);
	static std::string status_str(int status);

	chain_filter filter_chain;

private:
	/**
	 * generate response header stream.
	 *
	 * \note The buffers do not own the underlying memory blocks,
	 * therefore the response object must remain valid and
	 * not be changed until the write operation has completed.
	 */
	source_ptr serialize();

	source_ptr make_default_content();

	void finished(const asio::error_code& e);
};

// {{{ inline implementation
inline request& response::request() const
{
	return *request_;
}

inline bool response::headers_sent() const
{
	return headers_sent_;
}

inline void response::write(const source_ptr& content, const completion_handler_type& handler)
{
	if (headers_sent_)
		write_content(content, handler);
	else
		connection_->async_write(serialize(), 
			boost::bind(&response::complete_write, this, asio::placeholders::error, content, handler));
}

/** is invoked as completion handler when sending response headers. */
inline void response::complete_write(const asio::error_code& ec, const source_ptr& content, const completion_handler_type& handler)
{
	headers_sent_ = true;

	if (!ec)
	{
		// write response content
		write_content(content, handler);
	}
	else
	{
		// an error occured -> notify completion handler about the error
		handler(ec, 0);
	}
}

inline void response::write_content(const source_ptr& content, const completion_handler_type& handler)
{
	source_ptr filtered(new filter_source(content, filter_chain));

	connection_->async_write(filtered, handler);
}
// }}}

//@}

} // namespace x0

#endif
