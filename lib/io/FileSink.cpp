/* <x0/FileSink.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.io/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/FileSink.h>
#include <x0/io/SinkVisitor.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

FileSink::FileSink(const std::string& filename, int flags)
{
	handle_ = open(filename.c_str(), flags, 0666);
	if (handle_ >= 0) {
		fcntl(handle_, F_SETFD, fcntl(handle_, F_GETFD) | FD_CLOEXEC);
	}
}

FileSink::~FileSink()
{
	if (handle_ >= 0) {
		::close(handle_);
	}
}

void FileSink::accept(SinkVisitor& v)
{
	v.visit(*this);
}

ssize_t FileSink::write(const void *buffer, size_t size)
{
	return ::write(handle_, buffer, size);
}

} // namespace x0
