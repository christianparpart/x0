/* <x0/connection.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_hpp
#define x0_connection_hpp (1)

#include <x0/connection.hpp>
#include <x0/request_parser.hpp>
#include <x0/io/sink.hpp>
#include <x0/io/source.hpp>
#include <x0/io/async_writer.hpp>
#include <x0/io/buffer.hpp>
#include <x0/request.hpp>
#include <x0/server.hpp>
#include <x0/property.hpp>
#include <x0/types.hpp>
#include <x0/api.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>
#include <asio.hpp>
#include <boost/bind.hpp>
#include <string>

namespace x0 {

//! \addtogroup core
//@{

/**
 * \brief represents an HTTP connection handling incoming requests.
 */
class connection :
	public boost::enable_shared_from_this<connection>,
	private boost::noncopyable
{
public:
	/**
	 * creates an HTTP connection object.
	 * \param srv a ptr to the server object this connection belongs to.
	 */
	explicit connection(x0::server& srv);

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

	value_property<bool> secure;				//!< true if this is a secure (HTTPS) connection, false otherwise.

	asio::ip::tcp::socket& socket();			//!< Retrieves a reference to the connection socket.
	x0::server& server();						//!< Retrieves a reference to the server instance.

	std::string client_ip() const;				//!< Retrieves the IP address of the remote end point (client).
	int client_port() const;					//!< Retrieves the TCP port numer of the remote end point (client).

private:
	friend class response;

	template<class CompletionHandler> class write_handler;

	void async_read_some();
	void read_timeout(const asio::error_code& ec);
	void handle_read(const asio::error_code& e, std::size_t bytes_transferred);

	/** write something into the connection stream.
	 * \param buffer the buffer of bytes to be written into the connection.
	 * \param handler the completion handler to invoke once the buffer has been either fully written or an error occured.
	 */
	void async_write(const source_ptr& buffer, const completion_handler_type& handler);

	void write_timeout(const asio::error_code& ec);
	void response_transmitted(const asio::error_code& e);

	x0::server& server_;					//!< server object owning this connection
	asio::ip::tcp::socket socket_;			//!< underlying communication socket
	asio::deadline_timer timer_;			//!< deadline timer for detecting read/write timeouts.

	mutable std::string client_ip_;			//!< internal cache to client ip
	mutable int client_port_;				//!< internal cache to client port

	// HTTP request
	buffer buffer_;							//!< buffer for incoming data.
	request *request_;						//!< currently parsed http request 
	request_parser request_parser_;			//!< http request parser

//	asio::strand strand_;					//!< request handler strand
};

template<class CompletionHandler>
class connection::write_handler
{
private:
	connection_ptr connection_;
	CompletionHandler handler_;

public:
	write_handler(connection_ptr connection, const CompletionHandler& handler) :
		connection_(connection), handler_(handler)
	{
	}

	~write_handler()
	{
	}

	void operator()(const asio::error_code& ec, int bytes_transferred)
	{
		connection_->timer_.cancel();
		handler_(ec, bytes_transferred);
	}
};

// {{{ inlines
inline asio::ip::tcp::socket& connection::socket()
{
	return socket_;
}

inline server& connection::server()
{
	return server_;
}

inline void connection::async_write(const source_ptr& buffer, const completion_handler_type& handler)
{
	if (server_.max_write_idle() != -1)
	{
		write_handler<completion_handler_type> writeHandler(shared_from_this(), handler);

		timer_.expires_from_now(boost::posix_time::seconds(server_.max_write_idle()));
		timer_.async_wait(boost::bind(&connection::write_timeout, shared_from_this(), asio::placeholders::error));

		x0::async_write(socket(), buffer, writeHandler);
	}
	else
	{
		x0::async_write(socket(), buffer, handler);
	}
}

// }}}

//@}

} // namespace x0

#endif
