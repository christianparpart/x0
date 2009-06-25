/* <x0/response.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_response_hpp
#define x0_response_hpp (1)

#include <x0/types.hpp>
#include <x0/property.hpp>
#include <x0/header.hpp>
#include <x0/composite_buffer.hpp>
#include <x0/connection.hpp>
#include <x0/debug.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <string>
#include <vector>

namespace x0 {

/**
 * \ingroup core
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
		ok = 200,
		created = 201,
		accepted = 202,
		no_content = 204,
		multiple_choices = 300,
		moved_permanently = 301,
		moved_temporarily = 302,
		not_modified = 304,
		bad_request = 400,
		unauthorized = 401,
		forbidden = 403,
		not_found = 404,
		internal_server_error = 500,
		not_implemented = 501,
		bad_gateway = 502,
		service_unavailable = 503
	};
	// }}}

private:
	/// reference to the connection this response belongs to.
	connection_ptr connection_;

	/// reference to the related request.
	x0::request *request_;

	/// the headers to be included in the response.
	std::vector<x0::header> headers;

	/**
	 * tests wether we've already started serializing this response.
	 *
	 * did we already start serializing this response? if so, subsequent calls to serialize() will only contain 
	 * new data being added.
	 */
	bool serializing_;

	template<class CompletionHandler> class writer // {{{
	{
	private:
//		response *response_;
		composite_buffer buffer_;
		boost::asio::ip::tcp::socket& socket_;
		CompletionHandler handler_;

	public:
		writer(
//			x0::response *response,
			composite_buffer buffer,
			boost::asio::ip::tcp::socket& socket,
			const CompletionHandler& handler)
		  :
//			response_(),
//			response_(response),
			buffer_(buffer),
			socket_(socket),
			handler_(handler)
		{
			//DEBUG("response.writer()");
		}

		writer(const writer& v) :
//			response_(v.response_),
			buffer_(v.buffer_),
			socket_(v.socket_),
			handler_(v.handler_)
		{
			//DEBUG("response.writer(copy)");
		}

		~writer()
		{
			//DEBUG("response.~writer()");
		}

		// on first call, the headers have been sent, so we can continue with sending chunks now
		void operator()(const boost::system::error_code& ec, std::size_t /*bytes_transferred*/)
		{
			DEBUG("response.writer.operator(ec): buffer.empty=%d", buffer_.empty());

			if (buffer_.empty())
			{
				handler_(ec);
			}
			else
			{
				buffer_.async_write(socket_, *this);
			}
		}
	};//}}}

public:
	/** Creates an empty response object.
	 *
	 * \param connection the connection this response is going to be transmitted through
	 * \param request the corresponding request object. <b>We take over ownership of it!</b>
	 * \param status initial response status code.
	 *
	 * */
	response(connection_ptr connection, x0::request *request, int _status = 0);

	~response();

	/** retrieves a reference to the corresponding request object. */
	x0::request& request() const;

	/// HTTP response status code.
	value_property<int> status;

	// {{{ header manipulation
	/** adds a response header. */
	response& operator+=(const x0::header& hd);

	/** sets a response header value (overwrites existing one if already defined). */
	response& operator*=(const x0::header& hd);

	/** checks wether given response header has been already defined. */
	bool has_header(const std::string& name) const;

	/** retrieves the value of a given header by name. */
	std::string header(const std::string& name) const;

	/** sets a response header */
	const std::string& header(const std::string& name, const std::string& value);
	// }}}

	/** retrieves the content length as constructed thus far. */
	size_t content_length() const;

	/** write a string value to response content. */
	void write(const std::string& value);

	/** write a buffer to response content. */
	void write(void *buffer, int size);

	/** write contents available in given system handle to response content.
	 *
	 * \param fd file descriptor to read contents from.
	 * \param offset initial read offset inside file descriptor.
	 * \param size number of bytes to read from fd and write to client.
	 * \param close indicates wether to close file descriptor upon completion.
	 */
	void write(int fd, off_t offset, size_t size, bool close);

	/** write given buffer to response content.
	 * \see composite_buffer
	 */
	void write(composite_buffer& buffer);

	/** asynchronously flushes this response to client connection.
	 * \param handler the completion handler to invoke once fully transmitted.
	 */
	template<class CompletionHandler>
	void flush(const CompletionHandler& handler)
	{
		DEBUG("response.flush(handler): serializing=%d", serializing_);
		writer<CompletionHandler> internalHandler
		(
			//this,
			serialize(),
			connection_->socket(),
			handler
		);

		connection_->socket().async_write_some(boost::asio::null_buffers(), internalHandler);
	}

	/** asynchronously flushes response to client connection.
	 *
	 * This response is considered <b>complete</b> once all data currently in queue is being transmitted.
	 *
	 * \note this response object has been deleted after the completion of this request.
	 * \see template<class CompletionHandler> void flush(const CompletionHandler& handler)
	 */
	void flush()
	{
		flush
		(
			boost::bind
			(
				&response::transmitted,
				this,
				boost::asio::placeholders::error
			)
		);
	}

public:
	static const char *status_cstr(int status);
	static std::string status_str(int status);

	composite_buffer content;

private:
	char status_buf[3];

	/**
	 * convert the response into a composite_buffer.
	 *
	 * The buffers do not own the underlying memory blocks,
	 * therefore the response object must remain valid and
	 * not be changed until the write operation has completed.
	 */
	composite_buffer serialize();

	void transmitted(const boost::system::error_code& e);
};

// {{{ inline implementation
inline request& response::request() const
{
	return *request_;
}

inline size_t response::content_length() const
{
	return content.size();
}

inline void response::write(const std::string& value)
{
	content.push_back(value);
}

inline void response::write(void *buffer, int size)
{
	content.push_back(buffer, size);
}

inline void response::write(int fd, off_t offset, size_t size, bool close)
{
	content.push_back(fd, offset, size, close);
}

inline void response::write(composite_buffer& buffer)
{
	content.push_back(buffer);
}
// }}}

} // namespace x0

#endif
