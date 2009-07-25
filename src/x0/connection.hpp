/* <x0/connection.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_connection_hpp
#define x0_connection_hpp (1)

#include <x0/connection.hpp>
#include <x0/composite_buffer.hpp>
#include <x0/composite_buffer_async_writer.hpp>
#include <x0/request.hpp>
#include <x0/server.hpp>
#include <x0/property.hpp>
#include <x0/debug.hpp>
#include <x0/types.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>
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
	 * \param srv a ptr to the server object this connection belongs to.
	 */
	connection(boost::asio::io_service& io_service, connection_manager& manager, x0::server& srv);

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
	x0::server& server();						//!< gets a reference to the server instance.

private:
	friend class response;

	template<class CompletionHandler> class write_handler;

	void async_read_some();
	void read_timeout(const boost::system::error_code& ec);
	void handle_read(const boost::system::error_code& e, std::size_t bytes_transferred);

	template<class CompletionHandler>
	void async_write(const composite_buffer& buffer, const CompletionHandler& handler);
	void write_timeout(const boost::system::error_code& ec);
	void response_transmitted(const boost::system::error_code& e);

	boost::asio::ip::tcp::socket socket_;
	connection_manager& connection_manager_;	//!< corresponding connection manager
	x0::server& server_;						//!< server object owning this connection
	boost::asio::deadline_timer timer_;			//!< deadline timer for detecting read/write timeouts.

	// HTTP request
	boost::array<char, 8192> buffer_;	//!< buffer for incoming data.
	request *request_;					//!< currently parsed http request 
	request::reader request_reader_;	//!< http request parser

	boost::asio::strand strand_;		//!< request handler strand
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

	void operator()(const boost::system::error_code& ec, int bytes_transferred)
	{
		connection_->timer_.cancel();
		handler_(ec, bytes_transferred);
	}
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

inline server& connection::server()
{
	return server_;
}

template<class CompletionHandler>
inline void connection::async_write(const composite_buffer& buffer, const CompletionHandler& handler)
{
	if (server_.max_write_idle() != -1)
	{
		write_handler<CompletionHandler> writeHandler(shared_from_this(), handler);

		timer_.expires_from_now(boost::posix_time::seconds(server_.max_write_idle()));
		timer_.async_wait(boost::bind(&connection::write_timeout, shared_from_this(), boost::asio::placeholders::error));

		x0::async_write(socket(), buffer, writeHandler);
	}
	else
	{
		x0::async_write(socket(), buffer, handler);
	}
}

// }}}

} // namespace x0

#endif
