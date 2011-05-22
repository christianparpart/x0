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

} // namespace x0
