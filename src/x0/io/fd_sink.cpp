#include <x0/io/fd_sink.hpp>
#include <x0/io/source.hpp>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

fd_sink::fd_sink(int fd) :
	buf_(),
	offset_(0),
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

ssize_t fd_sink::pump(source& src)
{
	if (buf_.empty())
		src.pull(buf_);

	std::size_t remaining = buf_.size() - offset_;
	if (!remaining)
		return 0;

	ssize_t nwritten = ::write(handle_, buf_.data() + offset_, remaining);

	if (nwritten != -1)
	{
		if (static_cast<std::size_t>(nwritten) == remaining)
		{
			buf_.clear();
			offset_ = 0;
		}
		else
			offset_ += nwritten;
	}

	return nwritten;
}

} // namespace x0
