/* <x0/server.hpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_server_hpp
#define x0_server_hpp (1)

#include <x0/types.hpp>
#include <x0/api.hpp>

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <string>

namespace x0 {

class server;

/**
 * \ingroup core
 * \brief TCP/IP listener for the HTTP protocol.
 *
 * This class implements the TCP/IP listener for the HTTP protocol.
 * It binds and listens on a given address:port pair and creates a
 * new connection object for each new incoming TCP/IP client to process
 * all requests incoming from this request.
 *
 * @see server
 * @see connection
 */
class listener :
	public boost::noncopyable
{
public:
	explicit listener(server& srv);
	~listener();

	void configure(const std::string& address = "0::0", int port = 8080);
	void start();

	std::string address() const;
	int port() const;

private:
	/// handle completion of an async accept operation
	void handle_accept(const boost::system::error_code& e);

	x0::server& server_;
	boost::asio::ip::tcp::acceptor acceptor_;
	request_handler_fn handler_;
	connection_ptr new_connection_;
	std::string address_;
	int port_;
};

typedef boost::shared_ptr<listener> listener_ptr;

} // namespace x0

#endif
