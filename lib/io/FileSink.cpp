/* <x0/FileSink.cpp>
 *
 * This file is part of the x0 web server project and is released under LGPL-3.
 * http://www.xzero.ws/
 *
 * (c) 2009-2010 Christian Parpart <trapni@gentoo.org>
 */

#include <x0/io/FileSink.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace x0 {

FileSink::FileSink(const std::string& filename) :
	SystemSink(open(filename.c_str(), O_WRONLY | O_CREAT, 0666))
{
	fcntl(handle_, F_SETFD, fcntl(handle_, F_GETFD) | FD_CLOEXEC);
}

FileSink::~FileSink()
{
	::close(handle_);
}

} // namespace x0
