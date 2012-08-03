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
