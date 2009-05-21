/* <x0/vhost_selector.hpp>
 *
 * This file is part of the x0 web server, released under GPLv3.
 * (c) 2009 Chrisitan Parpart <trapni@gentoo.org>
 */

#ifndef x0_vhost_selector_hpp
#define x0_vhost_selector_hpp (1)

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>

namespace x0 {

struct vhost_selector
{
	std::string hostname;
	int port;

	explicit vhost_selector(const std::string _host) :
		hostname(_host), port(80)
	{
		std::size_t n = hostname.find(":");
		if (n != std::string::npos)
		{
			port = lexical_cast<int>(_host.substr(n + 1));
			hostname = _host.substr(0, n);
		}
	}

	vhost_selector(const std::string _host, int _port) :
		hostname(_host), port(_port)
	{
		std::size_t n = hostname.find(":");
		if (n != std::string::npos)
		{
			hostname = _host.substr(0, n);
		}
	}

	friend int compare(const vhost_selector& a, const vhost_selector& b)
	{
		if (&a == &b)
			return 0;

		if (a.hostname < b.hostname)
			return -1;

		if (a.hostname > b.hostname)
			return 1;

		if (a.port < b.port)
			return -1;

		if (a.port > b.port)
			return 1;

		return 0;
	}

	friend bool operator<(const vhost_selector& a, const vhost_selector& b)
	{
		return compare(a, b) < 0;
	}

	friend bool operator==(const vhost_selector& a, const vhost_selector& b)
	{
		return compare(a, b) == 0;
	}

	friend std::ostream& operator<<(std::ostream& os, const vhost_selector& vhs)
	{
		os << vhs.hostname << ":" << vhs.port;
		return os;
	}
};

} // namespace x0

#endif
