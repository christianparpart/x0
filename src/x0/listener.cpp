/* <x0/listener.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/listener.hpp>
#include <x0/connection.hpp>
#include <x0/server.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

namespace x0 {

listener::listener(server& srv)
  : server_(srv),
	acceptor_(server_.io_service_pool().get_service()),
	new_connection_(new connection(server_)),
	address_(),
	port_(-1)
{
}

listener::~listener()
{
	acceptor_.close();
}

void listener::configure(const std::string& address, int port)
{
	address_ = address;
	port_ = port;
}

void listener::start()
{
	boost::asio::ip::tcp::resolver resolver(server_.io_service_pool().get_service());
	boost::asio::ip::tcp::resolver::query query(address_, boost::lexical_cast<std::string>(port_));
	boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

	acceptor_.open(endpoint.protocol());

	acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::linger(false, 0));
	acceptor_.set_option(boost::asio::ip::tcp::no_delay(true));
	acceptor_.set_option(boost::asio::ip::tcp::acceptor::keep_alive(true));

	acceptor_.bind(endpoint);

	acceptor_.listen();

	acceptor_.async_accept(new_connection_->socket(),
		bind(&listener::handle_accept, this, boost::asio::placeholders::error));
}

void listener::handle_accept(const boost::system::error_code& e)
{
	if (!e)
	{
		new_connection_->start();

		new_connection_.reset(new connection(server_));
		acceptor_.async_accept(new_connection_->socket(),
			bind(&listener::handle_accept, this, boost::asio::placeholders::error));
	}
}

std::string listener::address() const
{
	return address_;
}

int listener::port() const
{
	return port_;
}

} // namespace x0
