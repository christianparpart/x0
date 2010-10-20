/* <SocketDriver.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_SocketDriver_hpp
#define sw_x0_SocketDriver_hpp (1)

#include <ev++.h>
#include <system_error>
#include <unistd.h>

namespace x0 {

class Socket;

class SocketDriver
{
public:
	SocketDriver();
	virtual ~SocketDriver();

	virtual bool isSecure() const;
	virtual Socket *create(int handle, struct ev_loop *loop);
	virtual void destroy(Socket *);
};

} // namespace x0

#endif
