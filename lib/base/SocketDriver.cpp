/* <x0/SocketDriver.cpp>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2014 Christian Parpart <trapni@gmail.com>
 */

#include <x0/SocketDriver.h>
#include <x0/Socket.h>
#include <x0/IPAddress.h>
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

Socket *SocketDriver::create(struct ev_loop *loop, IPAddress *ipaddr, int port)
{
	int fd = ::socket(ipaddr->family(), SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return nullptr;
	}

	char buf[sizeof(sockaddr_in6)];
	memset(&buf, 0, sizeof(buf));
	int rv = -1;
	switch (ipaddr->family()) {
		case IPAddress::V4: {
			sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_port = htons(port);
			sin.sin_family = AF_INET;
			memcpy(&sin.sin_addr, ipaddr->data(), ipaddr->size());
			rv = ::connect(fd, (sockaddr*)&sin, sizeof(sin));
			break;
		}
		case IPAddress::V6: {
			sockaddr_in6 sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin6_port = htons(port);
			sin.sin6_family = AF_INET6;
			memcpy(&sin.sin6_addr, ipaddr->data(), ipaddr->size());
			rv = ::connect(fd, (sockaddr*)&sin, sizeof(sin));
			break;
		}
		default:
			break;
	}

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
