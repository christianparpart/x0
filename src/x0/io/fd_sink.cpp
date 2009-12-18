#include <x0/io/fd_sink.hpp>

namespace x0 {

fd_sink::fd_sink(int fd) :
	handle_(fd)
{
}

void fd_sink::push(const chunk& data)
{
	;//! \todo read as much bytes as possible in one go, assume O_NONBLOCK
}

} // namespace x0
