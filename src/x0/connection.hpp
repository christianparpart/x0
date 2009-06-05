/* <x0/connection.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_hpp
#define x0_connection_hpp (1)

#include <x0/connection.hpp>
#include <x0/response.hpp>
#include <x0/request.hpp>
#include <x0/request_parser.hpp>
#include <x0/types.hpp>

#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
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

	/** get the connection socket handle.
	 */
	boost::asio::ip::tcp::socket& socket();

	/** start first async operation for this connection.
	 *
	 * This is done by simply registering the underlying socket to the the I/O service
	 * to watch for available input.
	 * \see stop()
	 */
	void start();

	/** stop all async operations associated with this connection.
	 *
	 * This is simply done by closing the underlying socket connection.
	 * \see start()
	 */
	void stop();

private:
	void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);
	void handle_write(const boost::system::error_code& e);

	boost::asio::ip::tcp::socket socket_;		//!< the socket handle
	connection_manager& connection_manager_;	//!< corresponding connection manager
	request_handler_fn request_handler_;		//!< request handler to use

	// HTTP request
	boost::array<char, 8192> buffer_;	//!< buffer for incoming data.
	request request_;					//!< parsed http request 
	request_parser request_parser_;		//!< http request parser

	response_ptr response_;				//!< response object for the current request within this connection
};

} // namespace x0

#endif
