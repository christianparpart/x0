#include <x0/fd_sink.hpp>
#include <x0/source.hpp>

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

std::size_t fd_sink::pump(source& src)
{
	if (buf_.empty())
		src.pull(buf_);

	ssize_t nwritten = ::write(handle_, buf_.data(), buf_.size());

	if (static_cast<std::size_t>(nwritten) == buf_.size())
		buf_.clear();

	return static_cast<std::size_t>(nwritten);
}

} // namespace x0
