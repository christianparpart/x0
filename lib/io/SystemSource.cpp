/* <x0/SystemSource.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/SystemSource.h>
#include <x0/io/SourceVisitor.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

namespace x0 {

void SystemSource::async(bool value)
{
	fcntl(handle_, F_SETFL, O_NONBLOCK, value ? 1 : 0);
}

bool SystemSource::async() const
{
	return fcntl(handle_, F_GETFL, O_NONBLOCK) > 0;
}

BufferRef SystemSource::pull(Buffer& buf)
{
	const std::size_t left = buf.size();
	const std::size_t count = std::min(static_cast<std::size_t>(Buffer::CHUNK_SIZE), count_);

	buf.reserve(left + count);

	if (offset_ != -1)
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

	return BufferRef();
}

void SystemSource::accept(SourceVisitor& v)
{
	v.visit(*this);
}

} // namespace x0
