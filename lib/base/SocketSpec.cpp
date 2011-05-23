#include <x0/SocketSpec.h>

namespace x0 {

SocketSpec::SocketSpec() :
	address(),
	port(-1),
	backlog(-1),
	valid(false)
{
}

void SocketSpec::clear()
{
	address = IPAddress();
	local.clear();

	port = -1;
	backlog = -1;
	valid = false;
}

std::string SocketSpec::str() const
{
	if (isLocal()) {
		return "unix:" + local;
	} else {
		char buf[256];
		snprintf(buf, sizeof(buf), "%s, port %d", address.str().c_str(), port);
		return buf;
	}
}

} // namespace x0
