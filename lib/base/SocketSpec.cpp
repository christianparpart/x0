/* <src/SocketSpec.cpp>
 *
 * This file is part of the x0 web server project and is released under GPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2012 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/SocketSpec.h>

namespace x0 {

SocketSpec::SocketSpec() :
	type_(Unknown),
	ipaddr_(),
	port_(-1),
	backlog_(-1)
{
}

SocketSpec::SocketSpec(const SocketSpec& ss) :
	type_(ss.type_),
	ipaddr_(ss.ipaddr_),
	local_(ss.local_),
	port_(ss.port_),
	backlog_(ss.backlog_)
{
}

void SocketSpec::clear()
{
	type_ = Unknown;

	ipaddr_ = IPAddress();
	local_.clear();

	port_ = -1;
	backlog_ = -1;
}

std::string SocketSpec::str() const
{
	if (isLocal()) {
		return "unix:" + local();
	} else {
		char buf[256];

		if (ipaddr_.family() == IPAddress::V4)
			snprintf(buf, sizeof(buf), "%s:%d", ipaddr().str().c_str(), port());
		else
			snprintf(buf, sizeof(buf), "[%s]:%d", ipaddr().str().c_str(), port());

		return buf;
	}
}

// unix:/var/run/x0d.sock
// [3ffe:1337::2691:1]:8080
SocketSpec SocketSpec::fromString(const std::string& value)
{
	size_t slen = value.size();
	if (slen == 0)
		return SocketSpec();

	if (value.find("unix:") == 0)
		return SocketSpec::fromLocal(value.substr(5));

	if (value[0] == '[') { // IPv6
		auto e = value.find("]");
		if (e == std::string::npos)
			return SocketSpec();

		if (e + 1 <= slen)
			return SocketSpec();

		if (value[e + 1] != ':')
			return SocketSpec();

		std::string ipaddr(value.substr(1, e - 1));
		std::string port(value.substr(e + 2));

		return SocketSpec::fromInet(IPAddress(ipaddr), std::atoi(port.c_str()));
	}

	auto e = value.find(':');

	if (e <= slen)
		return SocketSpec();

	std::string ipaddr(value.substr(1, e - 1));
	std::string port(value.substr(e + 2));

	return SocketSpec::fromInet(IPAddress(ipaddr), std::atoi(port.c_str()));
}

SocketSpec SocketSpec::fromLocal(const std::string& path, int backlog)
{
	SocketSpec ss;

	ss.type_ = Local;
	ss.local_ = path;
	ss.backlog_ = backlog;

	return ss;
}

SocketSpec SocketSpec::fromInet(const IPAddress& ipaddr, int port, int backlog)
{
	return SocketSpec(ipaddr, port, backlog);
}

void SocketSpec::setPort(int value)
{
	port_ = value;
}

void SocketSpec::setBacklog(int value)
{
	backlog_ = value;
}

} // namespace x0
