#include <x0/io/fd_source.hpp>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

fd_source::fd_source(int fd) :
	handle_(fd)
{
}

void fd_source::async(bool value)
{
	fcntl(handle_, F_SETFL, O_NONBLOCK, value ? 1 : 0);
}

bool fd_source::async() const
{
	return fcntl(handle_, F_GETFL, O_NONBLOCK) > 0;
}

buffer::view fd_source::pull(buffer& buf)
{
	const std::size_t pos = buf.size();
	const std::size_t rsize = buffer::CHUNK_SIZE / 4 * 1024 * 256;

	buf.reserve(pos + rsize);

	ssize_t nread = ::read(handle_, buf.begin() + pos, rsize);

	if (nread == -1)
		return buffer::view();

	buf.resize(pos + nread);

	return buf.sub(pos);
}

} // namespace x0
