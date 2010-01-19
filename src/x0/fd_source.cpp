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
	const std::size_t left = buf.size();
	const std::size_t count = std::min(static_cast<std::size_t>(buffer::CHUNK_SIZE), count_);

	buf.reserve(left + count);

	if (offset_ != static_cast<std::size_t>(-1))
	{
		ssize_t nread = ::pread(handle_, buf.end(), count, offset_);

		if (nread > 0)
		{
			offset_ += nread;
			count_ -= nread;

			buf.resize(left + nread);
			return buf.ref(left);
		}
	}
	else
	{
		ssize_t nread = ::read(handle_, buf.end(), count);

		if (nread > 0)
		{
			buf.resize(left + nread);
			return buf.ref(left);
		}
	}

	return buffer_ref();
}

void fd_source::accept(source_visitor& v)
{
	v.visit(*this);
}

} // namespace x0
