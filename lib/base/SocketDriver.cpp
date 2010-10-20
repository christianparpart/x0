/* <x0/SocketDriver.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/SocketDriver.h>
#include <x0/Socket.h>
#include <ev++.h>

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

Socket *SocketDriver::create(int handle, struct ev_loop *loop)
{
	return new Socket(loop, handle);
}

void SocketDriver::destroy(Socket *socket)
{
	delete socket;
}

} // namespace x0
