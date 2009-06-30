/* <x0/connection.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_hpp
#define x0_connection_hpp (1)

#include <x0/connection.hpp>
#include <x0/composite_buffer.hpp>
#include <x0/request.hpp>
#include <x0/request_reader.hpp>
#include <x0/property.hpp>
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

	/** Resumes async operations.
	 *
	 * This method is being invoked on a keep-alive connection to parse further requests.
	 * \see start()
	 */
	void resume();

	/** stop all async operations associated with this connection.
	 *
	 * This is simply done by closing the underlying socket connection.
	 * \see start()
	 */
	void stop();

	/** true if this is a secure (HTTPS) connection, false otherwise. */
	value_property<bool> secure;

	boost::asio::ip::tcp::socket& socket();		//!< get the connection socket handle.
	connection_manager& manager();				//!< corresponding connection manager

private:
	void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);
	void response_transmitted(const boost::system::error_code& e);

	boost::asio::ip::tcp::socket socket_;
	connection_manager& connection_manager_;	//!< corresponding connection manager
	request_handler_fn request_handler_;		//!< request handler to use

	// HTTP request
	boost::array<char, 8192> buffer_;	//!< buffer for incoming data.
	request *request_;					//!< currently parsed http request 
	request_reader request_reader_;		//!< http request parser

	boost::asio::strand strand_;		//!< request handler strand
};

// {{{ inlines
inline boost::asio::ip::tcp::socket& connection::socket()
{
	return socket_;
}

inline connection_manager& connection::manager()
{
	return connection_manager_;
}
// }}}

} // namespace x0

#endif
