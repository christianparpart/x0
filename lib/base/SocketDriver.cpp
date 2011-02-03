/* <x0/SocketDriver.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/SocketDriver.h>
#include <x0/Socket.h>
#include <flow/IPAddress.h>
#include <ev++.h>

#include <sys/socket.h>

namespace x0 {

SocketDriver::SocketDriver()
{
}

SocketDriver::~SocketDriver()
{
}

bool SocketDriver::isSecure() const
{
	return false;
}

Socket *SocketDriver::create(struct ev_loop *loop, int handle, int af)
{
	return new Socket(loop, handle, af);
}

Socket *SocketDriver::create(struct ev_loop *loop, Flow::IPAddress *ipaddr, int port)
{
	int fd = ::socket(ipaddr->family(), SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return nullptr;
	}

	char buf[sizeof(sockaddr_in6)];
	std::size_t size;
	memset(&buf, 0, sizeof(buf));
	switch (ipaddr->family()) {
		case Flow::IPAddress::V4:
			size = sizeof(sockaddr_in);
			((sockaddr_in *)buf)->sin_port = htons(port);
			((sockaddr_in *)buf)->sin_family = AF_INET;
			memcpy(&((sockaddr_in *)buf)->sin_addr, ipaddr->data(), ipaddr->size());
			break;
		case Flow::IPAddress::V6:
			size = sizeof(sockaddr_in6);
			((sockaddr_in6 *)buf)->sin6_port = htons(port);
			((sockaddr_in6 *)buf)->sin6_family = AF_INET6;
			memcpy(&((sockaddr_in6 *)buf)->sin6_addr, ipaddr->data(), ipaddr->size());
			break;
		default:
			::close(fd);
			return nullptr;
	}

	int rv = ::connect(fd, (const sockaddr *)buf, size);
	if (rv < 0) {
		::close(fd);
		perror("connect");
		return nullptr;
	}

	return new Socket(loop, fd, ipaddr->family());
}

void SocketDriver::destroy(Socket *socket)
{
	delete socket;
}

} // namespace x0
