#include <x0/io/SystemSink.h>
#include <x0/io/Source.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

SystemSink::SystemSink(int fd) :
	buf_(),
	offset_(0),
	handle_(fd)
{
}

void SystemSink::async(bool value)
{
	fcntl(handle_, F_SETFL, O_NONBLOCK, value ? 1 : 0);
}

bool SystemSink::async() const
{
	return fcntl(handle_, F_GETFL, O_NONBLOCK) > 0;
}

ssize_t SystemSink::pump(Source& src)
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
