#include <x0/fd_source.hpp>
#include <x0/source_visitor.hpp>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

void fd_source::async(bool value)
{
	fcntl(handle_, F_SETFL, O_NONBLOCK, value ? 1 : 0);
}

bool fd_source::async() const
{
	return fcntl(handle_, F_GETFL, O_NONBLOCK) > 0;
}

buffer_ref fd_source::pull(buffer& buf)
{
	const std::size_t pos = buf.size();
	const std::size_t rsize = buffer::CHUNK_SIZE;

	buf.reserve(pos + rsize);

	ssize_t nread = ::read(handle_, buf.begin() + pos, rsize);

	if (nread == -1)
		return buffer_ref();

	buf.resize(pos + nread);

	return buf.ref(pos);
}

void fd_source::accept(source_visitor& v)
{
	v.visit(*this);
}

} // namespace x0
