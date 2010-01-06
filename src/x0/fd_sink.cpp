#include <x0/fd_sink.hpp>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

fd_sink::fd_sink(int fd) :
	handle_(fd)
{
}

void fd_sink::async(bool value)
{
	fcntl(handle_, F_SETFL, O_NONBLOCK, value ? 1 : 0);
}

bool fd_sink::async() const
{
	return fcntl(handle_, F_GETFL, O_NONBLOCK) > 0;
}

buffer::view fd_sink::push(const buffer::view& buf)
{
	ssize_t nwritten = ::write(handle_, buf.data(), buf.size());

	if (static_cast<std::size_t>(nwritten) != buf.size())
		printf(" fd_sink: partial write: %ld/%ld bytes\n", nwritten, buf.size());

	if (nwritten != -1)
		return buf.sub(nwritten);
	else
		return buffer::view();
}

} // namespace x0
