#include <x0/SocketDriver.h>
#include <x0/Socket.h>
#include <ev++.h>

namespace x0 {

SocketDriver::SocketDriver(struct ev_loop *loop) :
	loop_(loop)
{
}

SocketDriver::~SocketDriver()
{
}

Socket *SocketDriver::create(int handle)
{
	return new Socket(loop_, handle);
}

void SocketDriver::destroy(Socket *socket)
{
	delete socket;
}

} // namespace x0
