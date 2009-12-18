/* <x0/listener.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/listener.hpp>
#include <x0/connection.hpp>
#include <x0/server.hpp>
#include <asio.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include <netinet/tcp.h> // TCP_QUICKACK, TCP_DEFER_ACCEPT

namespace x0 {

listener::listener(server& srv)
  : server_(srv),
	acceptor_(server_.io_service()),
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
	server_.log(severity::notice, "Start listening on %s:%d", address_.c_str(), port_);

	asio::ip::tcp::resolver resolver(server_.io_service());
	asio::ip::tcp::resolver::query query(address_, boost::lexical_cast<std::string>(port_));
	asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

	acceptor_.open(endpoint.protocol());

	acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
	acceptor_.set_option(asio::ip::tcp::acceptor::linger(false, 0));
	acceptor_.set_option(asio::ip::tcp::no_delay(true));
	acceptor_.set_option(asio::ip::tcp::acceptor::keep_alive(true));

#if 0
	acceptor_.set_option(asio::ip::tcp::acceptor::quick_ack(false));
#elif defined(TCP_QUICKACK)
	{
		int on = 0;
		if (setsockopt(acceptor_.native(), IPPROTO_TCP, TCP_QUICKACK, &on, sizeof(on)) < 0)
			throw std::runtime_error(strerror(errno));
	}
#endif

#if 0
	acceptor_.set_option(asio::ip::tcp::acceptor::defer_accept(true));
#elif defined(TCP_DEFER_ACCEPT)
	{
		int on = 1;
		if (setsockopt(acceptor_.native(), SOL_TCP, TCP_DEFER_ACCEPT, &on, sizeof(on)) < 0)
			throw std::runtime_error(strerror(errno));
	}
#endif

	acceptor_.bind(endpoint);

	acceptor_.listen();

	acceptor_.async_accept(new_connection_->socket(),
		bind(&listener::handle_accept, this, asio::placeholders::error));
}

void listener::handle_accept(const asio::error_code& e)
{
	if (!e)
	{
		new_connection_->start();

		new_connection_.reset(new connection(server_));
		acceptor_.async_accept(new_connection_->socket(),
			bind(&listener::handle_accept, this, asio::placeholders::error));
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
