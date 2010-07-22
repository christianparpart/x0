/* <x0/plugins/proxy/ProxyOrigin.h>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#ifndef sw_x0_proxy_ProxyOrigin_h
#define sw_x0_proxy_ProxyOrigin_h (1)

#include <string>
#include <ev++.h>
#include <unistd.h>
#include <netdb.h>

class ProxyOrigin
{
private:
	sockaddr_in sa_;
	std::string hostname_;
	int port_;
	bool enabled_;
	std::string error_;

public:
	ProxyOrigin();
	ProxyOrigin(const std::string& hostname, int port);

	const std::string& hostname() const;
	int port();

	const sockaddr *address() const;
	int size() const;

	void enable();
	bool is_enabled() const;
	void disable();

	std::string error() const;
};

#endif
