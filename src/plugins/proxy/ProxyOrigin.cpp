/* <x0/plugins/proxy/ProxyOrigin.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include "ProxyOrigin.h"

#include <x0/strutils.h>
#include <x0/Url.h>
#include <x0/Types.h>

#include <cstring>
#include <cstddef>
#include <cerrno>

#include <ev++.h>

#include <arpa/inet.h>		// inet_pton()
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

#if 1
#	define TRACE(msg...) /*!*/
#else
#	define TRACE(msg...) DEBUG("proxy: " msg)
#endif

inline ProxyOrigin::ProxyOrigin() :
	hostname_(),
	port_(),
	enabled_(false),
	error_()
{
	memset(&sa_, 0, sizeof(sa_));
}

inline ProxyOrigin::ProxyOrigin(const std::string& hostname, int port) :
	hostname_(hostname),
	port_(port),
	enabled_(true),
	error_()
{
	memset(&sa_, 0, sizeof(sa_));
	sa_.sin_family = AF_INET;
	sa_.sin_port = htons(port_);

	if (inet_pton(sa_.sin_family, hostname_.c_str(), &sa_.sin_addr) < 0)
	{
		error_ = strerror(errno);
		enabled_ = false;
	}
}

inline const std::string& ProxyOrigin::hostname() const
{
	return hostname_;
}

inline int ProxyOrigin::port()
{
	return port_;
}

inline const sockaddr *ProxyOrigin::address() const
{
	return reinterpret_cast<const sockaddr *>(&sa_);
}

inline int ProxyOrigin::size() const
{
	return sizeof(sa_);
}

inline void ProxyOrigin::enable()
{
	enabled_ = true;
}

inline bool ProxyOrigin::is_enabled() const
{
	return enabled_;
}

inline void ProxyOrigin::disable()
{
	enabled_ = false;
}

std::string ProxyOrigin::error() const
{
	return error_;
}
