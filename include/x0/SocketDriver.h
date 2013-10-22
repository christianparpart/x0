/* <SocketDriver.h>
 *
 * This file is part of the x0 web server project and is released under AGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2013 Christian Parpart <trapni@gmail.com>
 */

#ifndef sw_x0_SocketDriver_hpp
#define sw_x0_SocketDriver_hpp (1)

#include <ev++.h>
#include <system_error>
#include <unistd.h>
#include <x0/Api.h>

namespace x0 {

class Socket;
class IPAddress;

class X0_API SocketDriver
{
public:
	SocketDriver();
	virtual ~SocketDriver();

	virtual bool isSecure() const;
	virtual Socket *create(struct ev_loop *loop, int handle, int af);
	virtual Socket *create(struct ev_loop *loop, IPAddress* ipaddr, int port);
	virtual void destroy(Socket *);
};

} // namespace x0

#endif
