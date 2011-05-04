/* <x0/HttpListener.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/http/HttpListener.h>
#include <x0/http/HttpConnection.h>
#include <x0/http/HttpServer.h>
#include <x0/SocketDriver.h>
#include <x0/sysconfig.h>

#include <sd-daemon.h>

#include <arpa/inet.h>		// inet_pton()
#include <netinet/tcp.h>	// TCP_QUICKACK, TCP_DEFER_ACCEPT
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

namespace x0 {

#if !defined(NDEBUG)
#	define TRACE(msg...) (this->debug(msg))
#else
#	define TRACE(msg...) do {} while (0)
#endif

HttpListener::HttpListener(HttpServer& srv) : 
#ifndef NDEBUG
	Logging("HttpListener"),
#endif
	watcher_(srv.loop()),
	fd_(-1),
	addressFamily_(AF_UNSPEC),
	server_(srv),
	address_(),
	port_(-1),
	backlog_(SOMAXCONN),
	errors_(0),
	socketDriver_(new SocketDriver())
{
	watcher_.set<HttpListener, &HttpListener::callback>(this);
}

HttpListener::~HttpListener()
{
	TRACE("~HttpListener(): %s:%d", address_.c_str(), port_);
	stop();
}

void HttpListener::stop()
{
	TRACE("stopping");
	if (fd_ < 0)
		return;

	watcher_.stop();

	::close(fd_);
	fd_ = -1;

	setSocketDriver(nullptr);
}

inline void HttpListener::setsockopt(int socket, int layer, int option, int value)
{
	if (::setsockopt(socket, layer, option, &value, sizeof(value)) < 0)
	{
		log(Severity::error, "Error setting socket option (fd=%d, layer=%d, opt=%d, val=%d): %s",
				socket, layer, option, value, strerror(errno));
	}
}

void HttpListener::setSocketDriver(SocketDriver *sd)
{
	if (socketDriver_)
		delete socketDriver_;

	socketDriver_ = sd;
}

addrinfo *HttpListener::getAddressInfo(const char *address, int port)
{
	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	int rc;
	in6_addr saddr;

	if ((rc = inet_pton(AF_INET, address, &saddr)) == 1) {
		// valid IPv4 text address
		hints.ai_family = AF_INET;
		hints.ai_flags |= AI_NUMERICHOST;
	} else if ((rc = inet_pton(AF_INET6, address, &saddr)) == 1) {
		// valid IPv6 text address
		hints.ai_family = AF_INET6;
		hints.ai_flags |= AI_NUMERICHOST;
	}

	char sport[8];
	snprintf(sport, sizeof(sport), "%d", port);

	addrinfo *res;
	if ((rc = getaddrinfo(address, sport, &hints, &res)) != 0) {
		log(Severity::error, "Host not found: %s", gai_strerror(rc));
		return nullptr;
	}

	return res;
}

bool HttpListener::prepare()
{
	addrinfo *res = getAddressInfo(address_.c_str(), port_);
	if (res == nullptr)
		return false;

	// check systemd first
	int count = sd_listen_fds(false);
	if (count > 0) {
		fd_ = SD_LISTEN_FDS_START;
		int last = fd_ + count;

		for (addrinfo *ri = res; ri != nullptr; ri = ri->ai_next) {
			for (; fd_ < last; ++fd_) {
				if (sd_is_socket_inet(fd_, ri->ai_family, ri->ai_socktype, true, port_) > 0) {
					// matching file descriptor found
					goto done;
				}
			}
		}

		log(Severity::error, "No systemd file descriptor passed for bind address %s:%d",
				address_.c_str(), port_);
		goto err;
	}

	// create socket manually
	for (addrinfo *ri = res; ri != nullptr; ri = ri->ai_next) {
		fd_ = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);

		if (fd_ < 0)
			continue;

		if (fcntl(fd_, F_SETFD, fcntl(fd_, F_GETFD) | FD_CLOEXEC) < 0) {
			log(Severity::error, "Could not start listening on [%s]:%d. Error setting FD_CLOEXEC-flag: %s", address_.c_str(), port_, strerror(errno));
			goto err;
		}

		if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL) | O_NONBLOCK) < 0) {
			log(Severity::error, "Could not start listening on [%s]:%d. Error setting O_NONBLOCK-flag: %s", address_.c_str(), port_, strerror(errno));
			goto err;
		}

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

//		acceptor_.set_option(asio::ip::tcp::acceptor::linger(false, 0));
//		acceptor_.set_option(asio::ip::tcp::acceptor::keep_alive(true));

		if (::bind(fd_, res->ai_addr, res->ai_addrlen) < 0) {
			log(Severity::error, "Cannot bind to IP-address [%s]:%d: %s", address_.c_str(), port_, strerror(errno));
			goto err;
		}

		if (::listen(fd_, backlog_) < 0) {
			log(Severity::error, "Cannot listen to IP-address [%s]:%d: %s", address_.c_str(), port_, strerror(errno));
			goto err;
		}

		goto done;
	}

	// no proper listener-address found
	log(Severity::error, "Could not start listening on [%s]:%d. %s", address_.c_str(), port_,  strerror(errno));
	goto err;

done:
	addressFamily_ = res->ai_family;
	freeaddrinfo(res);

#if defined(WITH_SSL)
	if (isSecure())
		log(Severity::info, "Start listening on [%s]:%d [secure]", address_.c_str(), port_);
	else
		log(Severity::info, "Start listening on [%s]:%d", address_.c_str(), port_);
#else
	log(Severity::info, "Start listening on [%s]:%d", address_.c_str(), port_);
#endif

	return true;

err:
	if (fd_ >= 0) {
		::close(fd_);
		fd_ = -1;
	}

	if (res != nullptr)
		freeaddrinfo(res);

	return false;
}

bool HttpListener::start()
{
	if (fd_ < 0)
		return false;

	TRACE("starting");
	watcher_.start(fd_, ev::READ);
	return true;
}

void HttpListener::callback(ev::io& watcher, int revents)
{
	int fd;

#if defined(HAVE_ACCEPT4) && !defined(VALGRIND) // valgrind does not yet implement accept4()
	bool flagged = true;
	fd = ::accept4(handle(), nullptr, 0, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (fd < 0 && errno == ENOSYS) {
		fd = ::accept(handle(), nullptr, 0);
		flagged = false;
	}
#else
	bool flagged = false;
	fd = ::accept(handle(), nullptr, 0);
#endif

	if (fd < 0) {
		if (errno != EINTR && errno != EAGAIN)
			log(Severity::error, "Error accepting new connection socket: %s", strerror(errno));

		return;
	}

	if (!flagged) {
		int rv = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK | O_CLOEXEC);
		if (rv < 0) {
			log(Severity::error, "Error configuring new connection socket: %s", strerror(errno));
			::close(fd);
			return;
		}
	}

	server_.selectWorker()->enqueue(std::make_pair(fd, this));
}

std::string HttpListener::address() const
{
	return address_;
}

void HttpListener::address(const std::string& value)
{
	address_ = value;
}

int HttpListener::port() const
{
	return port_;
}

void HttpListener::port(int value)
{
	port_ = value;
}

int HttpListener::backlog() const
{
	return backlog_;
}

void HttpListener::backlog(int value)
{
	backlog_ = value;
}

} // namespace x0
