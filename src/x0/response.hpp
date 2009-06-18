/* <x0/response.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_response_hpp
#define x0_response_hpp (1)

#include <x0/types.hpp>
#include <x0/header.hpp>
#include <x0/composite_buffer.hpp>
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
 * this response contains all information of data to be sent back to the requesting client.
 *
 * \see request, connection, server
 */
struct response
{
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

	/// HTTP response status code.
	int status_;

	/// the headers to be included in the response.
	std::vector<x0::header> headers;

	/**
	 * convert the response into a composite_buffer.
	 *
	 * The buffers do not own the underlying memory blocks,
	 * therefore the response object must remain valid and
	 * not be changed until the write operation has completed.
	 */
	composite_buffer serialize();

	class write_handler // {{{
	{
	private:
		composite_buffer buffer_;
		boost::asio::ip::tcp::socket& socket_;
		boost::function<void(const boost::system::error_code&, std::size_t)> handler_;
		std::size_t total_transferred_;

	public:
		write_handler(
				composite_buffer buffer,
				boost::asio::ip::tcp::socket& socket,
				const boost::function<void(const boost::system::error_code&, std::size_t)>& handler) :
			buffer_(buffer), socket_(socket), handler_(handler), total_transferred_(0)
		{
		}

		// on first call, the headers have been sent, so we can start sending chunks now
		void operator()(const boost::system::error_code& ec, size_t bytes_transferred)
		{
			total_transferred_ += bytes_transferred;

			if (buffer_.empty())
			{
				handler_(ec, total_transferred_);
			}
			else
			{
				buffer_.async_write(socket_, *this);
			}
		}
	};//}}}

	void async_write(
		boost::asio::ip::tcp::socket& socket,
		const boost::function<void(const boost::system::error_code&, std::size_t)>& handler)
	{
		composite_buffer cb = serialize();
		write_handler internalWriteHandler(cb, socket, handler);
		socket.async_write_some(boost::asio::null_buffers(), internalWriteHandler);
	}

public:
	/** creates an empty response object */
	response();

	/** sets a standard response for given status code.
	 * \param value the response status code to initialize this response with.
	 */
	void status(int value);

	/** retrieves current status code. */
	int status() const;

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

public:
	static const char *status_cstr(int status);
	static std::string status_str(int status);

	composite_buffer content;
private:
	char status_buf[3];
};

// {{{ inline implementation

inline int response::status() const
{
	return status_;
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
