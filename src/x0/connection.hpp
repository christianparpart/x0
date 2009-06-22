/* <x0/connection.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_hpp
#define x0_connection_hpp (1)

#include <x0/connection.hpp>
#include <x0/composite_buffer.hpp>
#include <x0/response.hpp>
#include <x0/request.hpp>
#include <x0/request_parser.hpp>
#include <x0/debug.hpp>
#include <x0/types.hpp>

#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <string>

namespace x0 {

class connection_manager;

/**
 * \ingroup core
 * \brief represents an HTTP connection handling incoming requests.
 * \see connection_manager
 */
class connection :
	public boost::enable_shared_from_this<connection>,
	private boost::noncopyable
{
public:
	/**
	 * creates an HTTP connection object.
	 * \param io_service a reference to the I/O service object, used for handling I/O events.
	 * \param manager the connection manager, holding all served HTTP connections.
	 * \param handler the request handler to be invoked for incoming requests.
	 */
	connection(boost::asio::io_service& io_service,
		connection_manager& manager, const request_handler_fn& handler);

	~connection();

	/** start first async operation for this connection.
	 *
	 * This is done by simply registering the underlying socket to the the I/O service
	 * to watch for available input.
	 * \see stop()
	 * :
	 */
	void start();

	/** stop all async operations associated with this connection.
	 *
	 * This is simply done by closing the underlying socket connection.
	 * \see start()
	 */
	void stop();

	/** true if this is a secure (HTTPS) connection, false otherwise. */
	value_property<bool> secure;

	/** get the connection socket handle. */
	boost::asio::ip::tcp::socket& socket();

private:
	template<class CompletionHandler> class writer // {{{
	{
	private:
//		response *response_;
		composite_buffer buffer_;
		boost::asio::ip::tcp::socket& socket_;
		CompletionHandler handler_;
		std::size_t transfered_total_;
//		int i_;

	public:
		writer(
//				x0::response *response,
				composite_buffer buffer,
				boost::asio::ip::tcp::socket& socket,
				const CompletionHandler& handler) :
//			response_(),
//			response_(response),
			buffer_(buffer),
			socket_(socket),
			handler_(handler),
			transfered_total_(0)
//			i_(0)
		{
			DEBUG("connection.writer()");
		}

		writer(const writer& v) :
//			response_(v.response_),
			buffer_(v.buffer_),
			socket_(v.socket_),
			handler_(v.handler_),
			transfered_total_(v.transfered_total_)
//			i_(v.i_)
		{
			DEBUG("connection.writer(copy)");
		}

		~writer()
		{
			DEBUG("connection.~writer()");
		}

		// on first call, the headers have been sent, so we can start sending chunks now
		void operator()(const boost::system::error_code& ec, std::size_t bytes_transferred)
		{
			DEBUG("connection.writer.operator(ec, sz=%lu)", bytes_transferred);

			transfered_total_ += bytes_transferred;

			if (buffer_.empty())
			{
				handler_(ec);//, transfered_total_);
			}
			else
			{
				buffer_.async_write(socket_, *this);
			}
		}
	};//}}}

	void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);
	void response_transmitted(const boost::system::error_code& e);

	/** starts async write of given \p response.
	 * \param response the response object to transmit (taking over ownership)
	 * \param handler the completion handler to invoke once fully transmitted.
	 */
	template<class CompletionHandler>
	void async_write(x0::response *response, const CompletionHandler& handler)
	{
		writer<CompletionHandler> internalHandler
		(
			//response,
			response->serialize(),
			socket_,
			handler
		);

		socket_.async_write_some(boost::asio::null_buffers(), internalHandler);
	}

	/** starts async write of given \p response.
	 * \param response the response object to transmit (taking over ownership)
	 */
	void async_write(response *response)
	{
		async_write
		(
			response,
			boost::bind
			(
				&connection::response_transmitted,
				shared_from_this(),
				boost::asio::placeholders::error
			)
		);
	}

	boost::asio::ip::tcp::socket socket_;
	connection_manager& connection_manager_;	//!< corresponding connection manager
	request_handler_fn request_handler_;		//!< request handler to use

	// HTTP request
	boost::array<char, 8192> buffer_;	//!< buffer for incoming data.
	request *request_;					//!< currently parsed http request 
	request_parser request_parser_;		//!< http request parser
	response *response_;

	boost::asio::strand strand_;		//!< request handler strand
};

// {{{ inlines
inline boost::asio::ip::tcp::socket &connection::socket()
{
	return socket_;
}
// }}}

} // namespace x0

#endif
