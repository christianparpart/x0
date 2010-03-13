/* <x0/listener.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 *
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#include <x0/listener.hpp>
#include <x0/connection.hpp>
#include <x0/server.hpp>
#include <x0/sysconfig.h>

#include <arpa/inet.h>		// inet_pton()
#include <netinet/tcp.h>	// TCP_QUICKACK, TCP_DEFER_ACCEPT
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

namespace x0 {

listener::listener(x0::server& srv) : 
	watcher_(),
	fd_(-1),
	server_(srv),
	address_(),
	port_(-1),
	handler_()
{
	watcher_.set<listener, &listener::callback>(this);
}

listener::~listener()
{
	if (fd_ != -1)
		::close(fd_);
}

void listener::configure(const std::string& address, int port)
{
	address_ = address;
	port_ = port;
}

void listener::stop()
{
	watcher_.stop();
	::close(fd_);
}

inline void setsockopt(int socket, int layer, int option, int value)
{
	if (::setsockopt(socket, layer, option, &value, sizeof(value)) < 0)
		throw std::runtime_error(strerror(errno));
}

void listener::start()
{
	server_.log(severity::notice, "Start listening on %s:%d", address_.c_str(), port_);

	fd_ = ::socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
	fcntl(fd_, F_SETFL, FD_CLOEXEC);

	if (fcntl(fd_, F_SETFL, O_NONBLOCK) < 0)
		printf("could not set server socket into non-blocking mode: %s\n", strerror(errno));

	sockaddr_in6 sin;
	bzero(&sin, sizeof(sin));

	sin.sin6_family = AF_INET6;
	sin.sin6_port = htons(port_);

	if (inet_pton(sin.sin6_family, address_.c_str(), sin.sin6_addr.s6_addr) < 0)
		throw std::runtime_error(strerror(errno));

#if defined(SO_REUSEADDR)
	//! \todo SO_REUSEADDR: could be configurable
	setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, 1);
#endif

#if defined(TCP_QUICKACK)
	//! \todo TCP_QUICKACK: could be configurable
	setsockopt(fd_, SOL_TCP, TCP_QUICKACK, 1);
#endif

#if defined(TCP_DEFER_ACCEPT)
	setsockopt(fd_, SOL_TCP, TCP_DEFER_ACCEPT, 1);
#endif

//	acceptor_.set_option(asio::ip::tcp::acceptor::linger(false, 0));
//	acceptor_.set_option(asio::ip::tcp::acceptor::keep_alive(true));

	if (::bind(fd_, (sockaddr *)&sin, sizeof(sin)) < 0)
		throw std::runtime_error(strerror(errno));

	if (::listen(fd_, SOMAXCONN) < 0)
		throw std::runtime_error(strerror(errno));

	watcher_.start(fd_, ev::READ);
}

void listener::callback(ev::io& watcher, int revents)
{
#if defined(TCP_DEFER_ACCEPT)
	// it is ensured, that we have data pending, so directly start reading
	(new connection(*this))->handle_read();
#else
	// client connected, but we do not yet know if we have data pending
	(new connection(*this))->start();
#endif
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
