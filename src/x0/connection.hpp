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
#include <x0/request_handler.hpp>
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
 */
class connection :
	public enable_shared_from_this<connection>,
	private noncopyable {
public:
	connection(io_service& io_service,
		connection_manager& manager, const request_handler_fn& handler);

	~connection();

	/// get the connection socket handle
	ip::tcp::socket& socket();

	/// start first async operation for this connection
	void start();

	/// stop all async operations associated with this connection
	void stop();

private:
	void handle_read(const system::error_code& e, std::size_t bytes_transferred);
	void handle_write(const system::error_code& e);

	ip::tcp::socket socket_;					//!< the socket handle
	connection_manager& connection_manager_;	//!< corresponding connection manager
	request_handler_fn request_handler_;		//!< request handler to use

	// HTTP request
	array<char, 8192> buffer_;			//!< buffer for incoming data.
	request request_;					//!< parsed http request 
	request_parser request_parser_;		//!< http request parser

	response_ptr response_;
};

} // namespace x0

#endif
