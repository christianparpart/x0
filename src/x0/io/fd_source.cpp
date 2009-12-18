#include <x0/io/fd_source.hpp>

namespace x0 {

fd_source::fd_source(int fd) :
	handle_(fd)
{
}

chunk fd_source::pull()
{
	;//! \todo read as much bytes as possible in one go, assume O_NONBLOCK
}

} // namespace x0
